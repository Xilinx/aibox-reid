/*
 * Copyright 2021 Xilinx, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <math.h>
#include <vvas/vvas_kernel.h>
#include <gst/vvas/gstinferencemeta.h>
#define __STDC_FORMAT_MACROS 1
#include <stdint.h>


enum
{
  LOG_LEVEL_ERROR,
  LOG_LEVEL_WARNING,
  LOG_LEVEL_INFO,
  LOG_LEVEL_DEBUG
};

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define LOG_MESSAGE(level, ...) {\
  do {\
    char *str; \
    if (level == LOG_LEVEL_ERROR)\
      str = (char*)"ERROR";\
    else if (level == LOG_LEVEL_WARNING)\
      str = (char*)"WARNING";\
    else if (level == LOG_LEVEL_INFO)\
      str = (char*)"INFO";\
    else if (level == LOG_LEVEL_DEBUG)\
      str = (char*)"DEBUG";\
    if (level <= log_level) {\
      printf("[%s %s:%d] %s: ",__FILENAME__, __func__, __LINE__, str);\
      printf(__VA_ARGS__);\
      printf("\n");\
    }\
  } while (0); \
}


int log_level = LOG_LEVEL_WARNING;

using namespace cv;
using namespace std;

#define MAX_CLASS_LEN 1024
#define MAX_LABEL_LEN 1024
#define MAX_ALLOWED_CLASS 20
#define MAX_ALLOWED_LABELS 20

struct color
{
  unsigned int blue;
  unsigned int green;
  unsigned int red;
};

struct vvass_xclassification
{
  color class_color;
  char class_name[MAX_CLASS_LEN];
};


struct vvas_xoverlaypriv
{
  float font_size;
  unsigned int font;
  int line_thickness;
  int y_offset;
  color label_color;
  char label_filter[MAX_ALLOWED_LABELS][MAX_LABEL_LEN];
  unsigned char label_filter_cnt;
  unsigned short classes_count;
  vvass_xclassification class_list[MAX_ALLOWED_CLASS];
};

/* Get y and uv color components corresponding to givne RGB color */
void
convert_rgb_to_yuv_clrs (color clr, unsigned char *y, unsigned short *uv)
{
  Mat YUVmat;
  Mat BGRmat (2, 2, CV_8UC3, Scalar (clr.red, clr.green, clr.blue));
  cvtColor (BGRmat, YUVmat, cv::COLOR_BGR2YUV_I420);
  *y = YUVmat.at < uchar > (0, 0);
  *uv = YUVmat.at < uchar > (2, 0) << 8 | YUVmat.at < uchar > (2, 1);
  return;
}

static void DrawReID( VVASFrame *inframe, vvas_xoverlaypriv *kpriv,
  int xmin, int xmax, int ymin, int ymax, uint64_t lable,
  Mat& lumaImg, Mat& chromaImg)
{
  /* Check whether the frame is NV12 or BGR and act accordingly */
  char label_s[256];
  sprintf(label_s, "%lu", lable);
  std::string label_string(label_s);

  if (inframe->props.fmt == VVAS_VFMT_Y_UV8_420)
  {
    unsigned char yScalar;
    unsigned short uvScalar;
    color clr = {255, 0, 0};
    convert_rgb_to_yuv_clrs(clr, &yScalar, &uvScalar);
    /* Draw rectangle on y an uv plane */
    int new_xmin = floor(xmin / 2) * 2;
    int new_ymin = floor(ymin / 2) * 2;
    int new_xmax = floor(xmax / 2) * 2;
    int new_ymax = floor(ymax / 2) * 2;
    int h = new_ymax - new_ymin;
    int w = new_xmax - new_xmin;

    /* Lets not draw anything when the origin is (0,0) */
    if (new_xmin || new_ymin)
    {
      rectangle(lumaImg, Point(new_xmin, new_ymin), Point(new_xmax, new_ymax),
                Scalar(yScalar), kpriv->line_thickness, 1, 0);
      rectangle(chromaImg, Point(new_xmin / 2, new_ymin / 2),
                Point(new_xmax / 2, new_ymax / 2),
                Scalar(uvScalar), kpriv->line_thickness, 1, 0);
    }
    {
      int baseline, y_offset = 0;
      Size textsize = getTextSize(label_string, kpriv->font, kpriv->font_size, 1, &baseline);
      if ((h < 1) && (w < 1))
      {
        if (kpriv->y_offset)
        {
          y_offset = kpriv->y_offset;
        }
        else
        {
          y_offset = (inframe->props.height * 0.10);
        }
      }
      /* Draw filled rectangle for labelling, both on y and uv plane */
      rectangle(lumaImg, Rect(Point(new_xmin, new_ymin - textsize.height), textsize),
                Scalar(yScalar), FILLED, 1, 0);
      textsize.height /= 2;
      textsize.width /= 2;
      rectangle(chromaImg, Rect(Point(new_xmin / 2, new_ymin / 2 - textsize.height), textsize),
                Scalar(uvScalar), FILLED, 1, 0);

      /* Draw label text on the filled rectanngle */
      convert_rgb_to_yuv_clrs(kpriv->label_color, &yScalar, &uvScalar);
      putText(lumaImg, label_string, cv::Point(new_xmin, new_ymin + y_offset), kpriv->font, kpriv->font_size,
              Scalar(yScalar), kpriv->line_thickness, 1);
      putText(chromaImg, label_string, cv::Point(new_xmin / 2, new_ymin / 2 + y_offset / 2), kpriv->font,
              kpriv->font_size / 2, Scalar(uvScalar), kpriv->line_thickness, 1);
    }
  }
}

extern "C"
{
  int32_t xlnx_kernel_init (VVASKernel * handle)
  {
    vvas_xoverlaypriv *kpriv =
        (vvas_xoverlaypriv *) malloc (sizeof (vvas_xoverlaypriv));
    memset (kpriv, 0, sizeof (vvas_xoverlaypriv));

    json_t *jconfig = handle->kernel_config;
    json_t *val, *karray = NULL, *classes = NULL;

    /* Initialize config params with default values */
    log_level = LOG_LEVEL_WARNING;
    kpriv->font_size = 0.5;
    kpriv->font = 0;
    kpriv->line_thickness = 1;
    kpriv->y_offset = 0;
    kpriv->label_color = {0, 0, 0};
    strcpy(kpriv->label_filter[0], "class");
    strcpy(kpriv->label_filter[1], "probability");
    kpriv->label_filter_cnt = 2;
    kpriv->classes_count = 0;

      val = json_object_get (jconfig, "debug_level");
    if (!val || !json_is_integer (val))
        log_level = LOG_LEVEL_WARNING;
    else
        log_level = json_integer_value (val);

      val = json_object_get (jconfig, "font_size");
    if (!val || !json_is_integer (val))
        kpriv->font_size = 0.5;
    else
        kpriv->font_size = json_integer_value (val);

      val = json_object_get (jconfig, "font");
    if (!val || !json_is_integer (val))
        kpriv->font = 0;
    else
        kpriv->font = json_integer_value (val);

      val = json_object_get (jconfig, "thickness");
    if (!val || !json_is_integer (val))
        kpriv->line_thickness = 1;
    else
        kpriv->line_thickness = json_integer_value (val);

      val = json_object_get (jconfig, "y_offset");
    if (!val || !json_is_integer (val))
        kpriv->y_offset = 0;
    else
        kpriv->y_offset = json_integer_value (val);

    /* get label color array */
      karray = json_object_get (jconfig, "label_color");
    if (!karray)
    {
      LOG_MESSAGE (LOG_LEVEL_ERROR, "failed to find label_color");
      return -1;
    } else
    {
      kpriv->label_color.blue =
          json_integer_value (json_object_get (karray, "blue"));
      kpriv->label_color.green =
          json_integer_value (json_object_get (karray, "green"));
      kpriv->label_color.red =
          json_integer_value (json_object_get (karray, "red"));
    }

    karray = json_object_get (jconfig, "label_filter");

    if (!json_is_array (karray)) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, "label_filter not found in the config\n");
      return -1;
    }
    kpriv->label_filter_cnt = 0;
    for (unsigned int index = 0; index < json_array_size (karray); index++) {
      strcpy (kpriv->label_filter[index],
          json_string_value (json_array_get (karray, index)));
      kpriv->label_filter_cnt++;
    }

    /* get classes array */
    karray = json_object_get (jconfig, "classes");
    if (!karray) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, "failed to find key labels");
      return -1;
    }

    if (!json_is_array (karray)) {
      LOG_MESSAGE (LOG_LEVEL_ERROR, "labels key is not of array type");
      return -1;
    }
    kpriv->classes_count = json_array_size (karray);
    for (unsigned int index = 0; index < kpriv->classes_count; index++) {
      classes = json_array_get (karray, index);
      if (!classes) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, "failed to get class object");
        return -1;
      }

      val = json_object_get (classes, "name");
      if (!json_is_string (val)) {
        LOG_MESSAGE (LOG_LEVEL_ERROR, "name is not found for array %d", index);
        return -1;
      } else {
        strncpy (kpriv->class_list[index].class_name,
            (char *) json_string_value (val), MAX_CLASS_LEN - 1);
        LOG_MESSAGE (LOG_LEVEL_DEBUG, "name %s",
            kpriv->class_list[index].class_name);
      }

      val = json_object_get (classes, "green");
      if (!val || !json_is_integer (val))
        kpriv->class_list[index].class_color.green = 0;
      else
        kpriv->class_list[index].class_color.green = json_integer_value (val);

      val = json_object_get (classes, "blue");
      if (!val || !json_is_integer (val))
        kpriv->class_list[index].class_color.blue = 0;
      else
        kpriv->class_list[index].class_color.blue = json_integer_value (val);

      val = json_object_get (classes, "red");
      if (!val || !json_is_integer (val))
        kpriv->class_list[index].class_color.red = 0;
      else
        kpriv->class_list[index].class_color.red = json_integer_value (val);
    }

    handle->kernel_priv = (void *) kpriv;
    return 0;
  }

  uint32_t xlnx_kernel_deinit (VVASKernel * handle)
  {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, "enter");
    vvas_xoverlaypriv *kpriv = (vvas_xoverlaypriv *) handle->kernel_priv;

    if (kpriv)
      free (kpriv);

    return 0;
  }


  uint32_t xlnx_kernel_start (VVASKernel * handle, int start,
      VVASFrame * input[MAX_NUM_OBJECT], VVASFrame * output[MAX_NUM_OBJECT])
  {
    VVASFrame *inframe = input[0];
    vvas_xoverlaypriv *kpriv = (vvas_xoverlaypriv *)handle->kernel_priv;

    if (inframe->props.fmt == VVAS_VFMT_Y_UV8_420)
    {
      LOG_MESSAGE(LOG_LEVEL_DEBUG, "Input frame is in NV12 format\n");
      Mat lumaImg(input[0]->props.height, input[0]->props.stride, CV_8UC1, (char *)inframe->vaddr[0]);
      Mat chromaImg(input[0]->props.height / 2, input[0]->props.stride / 2, CV_16UC1, (char *)inframe->vaddr[1]);

      GstInferenceMeta *infer_meta = ((GstInferenceMeta *)gst_buffer_get_meta((GstBuffer *)
                                                                inframe->app_priv,
                                                            gst_inference_meta_api_get_type()));
      if (infer_meta == NULL)
      {
          LOG_MESSAGE(LOG_LEVEL_INFO, "vvas meta data is not available for crop");
          return false;
      }

      GstInferencePrediction *root = infer_meta->prediction;

      /* Iterate through the immediate child predictions */
      GSList *tmp = gst_inference_prediction_get_children(root);
      for (GSList *child_predictions = tmp;
           child_predictions;
           child_predictions = g_slist_next(child_predictions))
      {
          GstInferencePrediction *child = (GstInferencePrediction *)child_predictions->data;

          /* On each children, iterate through the different associated classes */
          for (GList *classes = child->classifications;
               classes; classes = g_list_next(classes))
          {
              GstInferenceClassification *classification = (GstInferenceClassification *)classes->data;
              if ((int64_t)child->reserved_2 != -1) 
              {
              DrawReID( inframe, kpriv,
                        child->bbox.x, child->bbox.x + child->bbox.width,
                        child->bbox.y, child->bbox.y + child->bbox.height,
                        (uint64_t)child->reserved_1, lumaImg, chromaImg);
              }
          }
      }
      g_slist_free(tmp);
      return 0;
    }
    else
    {
      LOG_MESSAGE(LOG_LEVEL_WARNING, "Unsupported color format\n");
      return 0;
    }
  }

  int32_t xlnx_kernel_done (VVASKernel * handle)
  {
    LOG_MESSAGE (LOG_LEVEL_DEBUG, "enter");
    return 0;
  }
}
