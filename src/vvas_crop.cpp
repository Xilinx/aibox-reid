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

#include <stdio.h>
#include <stdint.h>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <thread>
extern "C"
{
#include <vvas/vvas_kernel.h>
#include <gst/vvas/gstinferencemeta.h>
}

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
static int log_level = LOG_LEVEL_WARNING;



#define MAX_CHANNELS 40
//#define PROFILING 1
#define FRAME_SIZE(w,h) ((w)*(h)*3) // frame size for RGB

struct _roi {
    uint32_t y_cord;
    uint32_t x_cord;
    uint32_t height;
    uint32_t width;
    double   prob;
	GstInferencePrediction *prediction;
};

typedef struct _vvas_ms_roi {
    uint32_t nobj;
    struct _roi roi[MAX_CHANNELS];
} vvas_ms_roi;


static uint32_t xlnx_multiscaler_align(uint32_t stride_in, uint16_t AXIMMDataWidth) {
    uint32_t stride;
    uint16_t MMWidthBytes = AXIMMDataWidth / 8;

    stride = ((( stride_in ) + MMWidthBytes - 1) / MMWidthBytes) * MMWidthBytes;
    return stride;
}

using namespace std;
using namespace cv;

static inline GstVideoFormat
get_gst_format (VVASVideoFormat kernel_fmt)
{
  switch (kernel_fmt) {
    case VVAS_VFMT_Y8:
      return GST_VIDEO_FORMAT_GRAY8;
    case VVAS_VFMT_Y_UV8_420:
      return GST_VIDEO_FORMAT_NV12;
    case VVAS_VFMT_BGR8:
      return GST_VIDEO_FORMAT_BGR;
    case VVAS_VFMT_RGB8:
      return GST_VIDEO_FORMAT_RGB;
    case VVAS_VFMT_YUYV8:
      return GST_VIDEO_FORMAT_YUY2;
    case VVAS_VFMT_RGBX10:
      return GST_VIDEO_FORMAT_r210;
    case VVAS_VFMT_YUV8:
      return GST_VIDEO_FORMAT_v308;
    case VVAS_VFMT_Y10:
      return GST_VIDEO_FORMAT_GRAY10_LE32;
    case VVAS_VFMT_ABGR8:
      return GST_VIDEO_FORMAT_ABGR;
    case VVAS_VFMT_ARGB8:
      return GST_VIDEO_FORMAT_ARGB;
    default:
      GST_ERROR ("Not supporting kernel format %d yet", kernel_fmt);
      return GST_VIDEO_FORMAT_UNKNOWN;
  }
}

static int Crop_one_bgr(
    VVASKernel *handle,
    VVASFrame *input[MAX_NUM_OBJECT],
    const vvas_ms_roi& roi_data,
    const Mat& bgrImg,
    int ind
)
{
    VVASFrameProps out_props = {0, };
    out_props.width = 80;
    out_props.height = 176;
    out_props.fmt = VVAS_VFMT_BGR8;
    uint32_t size = FRAME_SIZE(out_props.width, out_props.height);

    GstBuffer *newBuf = gst_buffer_new_allocate(NULL, size, NULL);
    gst_buffer_add_video_meta (newBuf, GST_VIDEO_FRAME_FLAG_NONE,
        get_gst_format (out_props.fmt), out_props.width, out_props.height);

    {
        {
            roi_data.roi[ind].prediction->sub_buffer = newBuf;
            {
                cv::Rect ROI(roi_data.roi[ind].x_cord, roi_data.roi[ind].y_cord,
                              roi_data.roi[ind].width, roi_data.roi[ind].height);

                cv::Mat subbgr = bgrImg(ROI);

                GstMapInfo info;
                gst_buffer_map(newBuf, &info, GST_MAP_WRITE);
                char *indata = (char *)info.data;
                cv::Mat subbgrResize(out_props.height, out_props.width, CV_8UC3, indata);
                resize(subbgr, subbgrResize, subbgrResize.size());

                gst_buffer_unmap(newBuf, &info);
            }
        }
        return 0;
    }
}

static int Crop_range_bgr(
    VVASKernel *handle,
    VVASFrame *input[MAX_NUM_OBJECT],
    const vvas_ms_roi& roi_data,
    const Mat& bgrImg,
    int start, int stop
)
{
    cv::Mat bgrClone=bgrImg.clone();
    for (int i = start; i < stop; i++)
    {
        Crop_one_bgr(handle, input, roi_data, bgrClone, i);
    }
    return 0;
}

void
Thread(int numThread, int start, int stop, std::function<void(int, int)> func )
{
    int totalLoop = stop - start;
    int nloopPerT = totalLoop / numThread;
    int left = totalLoop % numThread;
    if (left > 0)
    {
        nloopPerT += 1;
        left = totalLoop - nloopPerT * (numThread - 1);
    }
    else 
    {
        left = nloopPerT;
    }
    vector<thread> pool;
    for (int i = 0; i < numThread; i++)
    {
        pool.emplace_back( func, start + nloopPerT * i,
                                     start + nloopPerT * i + ((i == numThread - 1) ? left : nloopPerT) );
    }
    for (int i = 0; i < numThread; i++)
    {
        pool[i].join();
    }
}

static int xlnx_multiscaler_descriptor_create (VVASKernel *handle,
    VVASFrame *input[MAX_NUM_OBJECT], VVASFrame *output[MAX_NUM_OBJECT],
    const vvas_ms_roi& roi_data)
{
    VVASFrameProps out_props = {0, };

    VVASFrame *in_vvas_frame = input[0];

    if (in_vvas_frame->props.fmt == VVAS_VFMT_BGR8)
    {
    LOG_MESSAGE(LOG_LEVEL_DEBUG, "Input frame is in BGR8 format\n");

    Mat bgrImg(input[0]->props.height, input[0]->props.width, CV_8UC3, (char *)in_vvas_frame->vaddr[0]);
    Crop_range_bgr(
        handle,
        input,
        roi_data,
        bgrImg,
        0, roi_data.nobj);
    }
    else
    {
        LOG_MESSAGE(LOG_LEVEL_WARNING, "Unsupported color format %d \n", in_vvas_frame->props.fmt);
        return 0;
    }
    return 0;
}


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
        LOG_MESSAGE(LOG_LEVEL_INFO, "vvas meta data is not available for crop");
        return false;
    }

    GstInferencePrediction *root = infer_meta->prediction;

    /* Iterate through the immediate child predictions */
    GSList *collects = gst_inference_prediction_get_children(root);
    for ( GSList *child_predictions = collects; child_predictions;
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
                roi_data.roi[ind].y_cord = (uint32_t)child->bbox.y + child->bbox.y % 2;
                roi_data.roi[ind].x_cord = (uint32_t)child->bbox.x;
                roi_data.roi[ind].height = (uint32_t)child->bbox.height - child->bbox.height % 2;
                roi_data.roi[ind].width = (uint32_t)child->bbox.width - child->bbox.width % 2;
                roi_data.roi[ind].prob = classification->class_prob;
                roi_data.roi[ind].prediction = child;
                roi_data.nobj++;
            }
        }
    }
    g_slist_free(collects);
    return 0;
}
extern "C"
{


int32_t xlnx_kernel_start (VVASKernel *handle, int start /*unused */,
        VVASFrame *input[MAX_NUM_OBJECT], VVASFrame *output[MAX_NUM_OBJECT])
{
    int ret;
    uint32_t value = 0;
    vvas_ms_roi roi_data;
    parse_rect(handle, start, input, output, roi_data);
   /* set descriptor */
    xlnx_multiscaler_descriptor_create (handle, input, output, roi_data);

    return 0;
}

int32_t xlnx_kernel_init (VVASKernel *handle)
{
    handle->is_multiprocess = 1;        
    return 0;
}

uint32_t xlnx_kernel_deinit (VVASKernel *handle)
{
    return 0;
}

int32_t xlnx_kernel_done(VVASKernel *handle)
{
    /* dummy */
    return 0;
}

}
