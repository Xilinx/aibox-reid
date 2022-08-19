/*
 * Copyright 2021 Xilinx Inc.
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

#include <gst/vvas/gstinferencemeta.h>
#include <vvas/vvas_kernel.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <vitis/ai/nnpp/reid.hpp>
#include <vitis/ai/reid.hpp>
#include <vitis/ai/reidtracker.hpp>
#include "common.hpp"
#include <sstream>

#define MAX_REID 20
#define DEFAULT_REID_THRESHOLD 0.2
#define DEFAULT_REID_DEBUG     0
#define DEFAULT_MODEL_NAME     "personreid-res18_pt"
#define DEFAULT_MODEL_PATH     "/opt/xilinx/kv260-aibox-reid/share/vitis_ai_library/models"

using namespace std;

struct _Face {
  int last_frame_seen;
  int xctr;
  int yctr;
  int id;
  cv::Mat features;
};

typedef struct _kern_priv {
  uint32_t debug;
  double threshold;
  std::string modelpath;
  std::string modelname;
  std::shared_ptr<vitis::ai::Reid> det;
  std::shared_ptr<vitis::ai::ReidTracker> tracker;
} ReidKernelPriv;

struct _roi {
    uint32_t y_cord;
    uint32_t x_cord;
    uint32_t height;
    uint32_t width;
    double   prob;
	  GstInferencePrediction *prediction;
};

#define MAX_CHANNELS 40
typedef struct _vvas_ms_roi {
    uint32_t nobj;
    struct _roi roi[MAX_CHANNELS];
} vvas_ms_roi;

static int parse_rect(VVASKernel * handle, int start,
      VVASFrame * input[MAX_NUM_OBJECT], VVASFrame * output[MAX_NUM_OBJECT],
      vvas_ms_roi &roi_data
      )
{
    VVASFrame *inframe = input[0];
    GstInferenceMeta *infer_meta = ((GstInferenceMeta *)gst_buffer_get_meta((GstBuffer *)
                                                              inframe->app_priv,
                                                          gst_inference_meta_api_get_type()));
    roi_data.nobj = 0;
    if (infer_meta == NULL)
    {
        return 0;
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
            if (roi_data.nobj < MAX_CHANNELS)
            {
                int ind = roi_data.nobj;
                struct _roi &roi = roi_data.roi[ind];
                roi.y_cord = (uint32_t)child->bbox.y + child->bbox.y % 2;
                roi.x_cord = (uint32_t)child->bbox.x;
                roi.height = (uint32_t)child->bbox.height - child->bbox.height % 2;
                roi.width = (uint32_t)child->bbox.width - child->bbox.width % 2;
                roi.prob = classification->class_prob;
                roi.prediction = child;
                roi_data.nobj++;

            }
        }
    }
    g_slist_free(tmp);
    return 0;
}

extern "C" {
int32_t xlnx_kernel_init(VVASKernel *handle) {
  json_t *jconfig = handle->kernel_config;
  json_t *val; /* kernel config from app */

  handle->is_multiprocess = 1;
  ReidKernelPriv *kernel_priv =
      (ReidKernelPriv *)calloc(1, sizeof(ReidKernelPriv));
  if (!kernel_priv) {
    printf("Error: Unable to allocate reID kernel memory\n");
  }

  /* parse config */
  val = json_object_get(jconfig, "threshold");
  if (!val || !json_is_number(val))
    kernel_priv->threshold = DEFAULT_REID_THRESHOLD;
  else
    kernel_priv->threshold = json_number_value(val);

  val = json_object_get(jconfig, "debug");
  if (!val || !json_is_number(val))
    kernel_priv->debug = DEFAULT_REID_DEBUG;
  else
    kernel_priv->debug = json_number_value(val);

  val = json_object_get(jconfig, "model-name");
  if (!val || !json_is_string (val))
    kernel_priv->modelname = DEFAULT_MODEL_NAME;
  else
    kernel_priv->modelname = (char *) json_string_value (val);

  val = json_object_get(jconfig, "model-path");
  if (!val || !json_is_string (val))
    kernel_priv->modelpath = DEFAULT_MODEL_PATH;
  else
    kernel_priv->modelpath = (char *) json_string_value (val);

  std::string xmodelfile = kernel_priv->modelpath + "/" + kernel_priv->modelname + "/" + kernel_priv->modelname + ".xmodel";
  kernel_priv->det = vitis::ai::Reid::create(xmodelfile);
  if (kernel_priv->det.get() == NULL) {
    printf("Error: Unable to create Reid runner with model %s.\n", xmodelfile.c_str());
  }
  kernel_priv->tracker = vitis::ai::ReidTracker::create();

  handle->kernel_priv = (void *)kernel_priv;
  return 0;
}

uint32_t xlnx_kernel_deinit(VVASKernel *handle) {
  ReidKernelPriv *kernel_priv = (ReidKernelPriv *)handle->kernel_priv;
  free(kernel_priv);
  return 0;
}

int32_t xlnx_kernel_start(VVASKernel *handle, int start /*unused */,
                          VVASFrame *input[MAX_NUM_OBJECT],
                          VVASFrame *output[MAX_NUM_OBJECT]) {
  VVASFrame *in_vvas_frame = input[0];
  ReidKernelPriv *kernel_priv = (ReidKernelPriv *)handle->kernel_priv;
  if ( !kernel_priv->det.get() || !kernel_priv->tracker.get() ) {
    return 1;
  }

  static int frame_num = 0;
  frame_num++;
  std::vector<vitis::ai::ReidTracker::InputCharact> input_characts;
  /* get metadata from input */

  vvas_ms_roi roi_data;
  parse_rect(handle, start, input, output, roi_data);

  m__TIC__(getfeat);
  for (uint32_t i = 0; i < roi_data.nobj; i++) {
    struct _roi& roi = roi_data.roi[i];
    {
      GstBuffer *buffer = (GstBuffer *)roi.prediction->sub_buffer; /* resized crop image*/
      int xctr = roi.x_cord + roi.width / 2;
      int yctr = roi.y_cord + roi.height / 2;
      GstMapInfo info;
      gst_buffer_map(buffer, &info, GST_MAP_READ);

      GstVideoMeta *vmeta = gst_buffer_get_video_meta(buffer);
      if (!vmeta) {
        printf("ERROR: VVAS REID: video meta not present in buffer");
      } else if (vmeta->width == 80 && vmeta->height == 176) {
        char *indata = (char *)info.data;
        cv::Mat image(vmeta->height, vmeta->width, CV_8UC3, indata);
        auto input_box =
            cv::Rect2f(roi.x_cord, roi.y_cord,
                       roi.width, roi.height);
        m__TIC__(reidrun);
        auto feat = kernel_priv->det->run(image).feat;
        m__TOC__(reidrun);
        m__TIC__(inputpush);
        input_characts.emplace_back(feat, input_box, roi.prob, -1, i);
        m__TOC__(inputpush);
        if (kernel_priv->debug == 2) {
            printf("Tracker input: Frame %d: obj_ind %d, xmin %u, ymin %u, xmax %u, ymax %u, prob: %f\n",
                    frame_num, i, roi.x_cord, roi.y_cord,
                       roi.x_cord + roi.width,
                       roi.y_cord + roi.height, roi.prob);
        }
      } else {
        printf("ERROR: VVAS REID: Invalid resolution for reid (%u x %u)\n",
               vmeta->width, vmeta->height);
      }
      gst_buffer_unmap(buffer, &info);
    }
  }
  m__TOC__(getfeat);
  if (input_characts.size() > 0)
  {
  std::vector<vitis::ai::ReidTracker::OutputCharact> track_results =
      std::vector<vitis::ai::ReidTracker::OutputCharact>(
          kernel_priv->tracker->track(frame_num, input_characts, true, true));
  if (kernel_priv->debug) {
      printf("Tracker result: \n");
  }
  int i = 0;
  for (auto &r : track_results) {
    auto box = get<1>(r);
    gint tmpx = box.x, tmpy = box.y;
    guint tmpw = box.width, tmph = box.height;
    uint64_t gid = get<0>(r);
    if (kernel_priv->debug) {
      printf("Frame %d: %" PRIu64 ", xmin %d, ymin %d, w %u, h %u\n",
         frame_num, gid,
         tmpx, tmpy,
         tmpw, tmph);
    }

    struct _roi& roi = roi_data.roi[i];
    roi_data.roi[i].prediction->bbox.x = tmpx;
    roi_data.roi[i].prediction->bbox.y = tmpy;
    roi_data.roi[i].prediction->bbox.width = tmpw;
    roi_data.roi[i].prediction->bbox.height = tmph;
    roi_data.roi[i].prediction->reserved_1 = (void*)gid;
    roi_data.roi[i].prediction->reserved_2 = (void*)1;

    i++;
  }

  for (; i < roi_data.nobj; i++)
  {
    roi_data.roi[i].prediction->reserved_2 = (void*)-1;
  }
  }
  return 0;
}

int32_t xlnx_kernel_done(VVASKernel *handle) {
  /* dummy */
  return 0;
}
}
