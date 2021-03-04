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
#include <ivas/ivas_kernel.h>
#include <gst/ivas/gstivasmeta.h>
#include <gst/ivas/gstinferencemeta.h>
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
};

typedef struct _ivas_ms_roi {
    uint32_t nobj;
    struct _roi roi[MAX_CHANNELS];
} ivas_ms_roi;


static uint32_t xlnx_multiscaler_align(uint32_t stride_in, uint16_t AXIMMDataWidth) {
    uint32_t stride;
    uint16_t MMWidthBytes = AXIMMDataWidth / 8;

    stride = ((( stride_in ) + MMWidthBytes - 1) / MMWidthBytes) * MMWidthBytes;
    return stride;
}

using namespace std;
using namespace cv;

static int Crop_one(
    IVASKernel *handle,
    IVASFrame *input[MAX_NUM_OBJECT],
    const ivas_ms_roi& roi_data,
    GstIvasMeta *ivas_meta,
    const Mat& lumaImg,
    const Mat& chromaImg,
    int ind
)
{
    IVASFrameProps out_props = {0, };
    out_props.width = 80;
    out_props.height = 160;
    out_props.fmt = IVAS_VFMT_BGR8;
    uint32_t size = FRAME_SIZE(out_props.width, out_props.height);

    IVASFrame *out_ivas_frame = ivas_alloc_buffer(handle, size, IVAS_INTERNAL_MEMORY, &out_props);
    if (!out_ivas_frame)
    {
        printf("ERROR: IVAS MS: failed to allocate frame memory");
        return 0;
    }
    else
    {
        IvasObjectMetadata *ivas_obj =
            (IvasObjectMetadata *)g_list_nth_data(ivas_meta->xmeta.objects, ind);
        if (ivas_obj)
        {
            ivas_obj->obj_list = g_list_append(ivas_obj->obj_list, out_ivas_frame->app_priv);

            ivas_obj->bbox_meta.xmin = roi_data.roi[ind].x_cord;
            ivas_obj->bbox_meta.xmax = roi_data.roi[ind].x_cord + roi_data.roi[ind].width;
            ivas_obj->bbox_meta.ymin = roi_data.roi[ind].y_cord;
            ivas_obj->bbox_meta.ymax = roi_data.roi[ind].y_cord + roi_data.roi[ind].height;
            ivas_obj->obj_prob = roi_data.roi[ind].prob;
            {
                cv::Rect yROI(roi_data.roi[ind].x_cord, roi_data.roi[ind].y_cord,
                              roi_data.roi[ind].width, roi_data.roi[ind].height);
                cv::Mat ycrop = lumaImg(yROI);
                cv::Rect cROI(roi_data.roi[ind].x_cord, roi_data.roi[ind].y_cord / 2,
                              roi_data.roi[ind].width, roi_data.roi[ind].height / 2);
                cv::Mat ccrop = chromaImg(cROI);
                cv::Mat sub12;
                vconcat(ycrop, ccrop, sub12);

                cv::Mat subbgr;
                cv::cvtColor(sub12, subbgr, COLOR_YUV2BGR_NV12);

                GstMapInfo info;
                gst_buffer_map((GstBuffer *)out_ivas_frame->app_priv, &info, GST_MAP_WRITE);
                char *indata = (char *)info.data;
                cv::Mat subbgrResize(out_props.height, out_props.width, CV_8UC3, indata);
                resize(subbgr, subbgrResize, subbgrResize.size());
                gst_buffer_unmap((GstBuffer *)out_ivas_frame->app_priv, &info);
            }
            out_ivas_frame->app_priv = NULL;
        }
        ivas_free_buffer(handle, out_ivas_frame);
        out_ivas_frame = NULL;
        return 0;
    }
}

static int Crop_range(
    IVASKernel *handle,
    IVASFrame *input[MAX_NUM_OBJECT],
    const ivas_ms_roi& roi_data,
    GstIvasMeta *ivas_meta,
    const Mat& lumaImg,
    const Mat& chromaImg,
    int start, int stop
)
{
    for (int i = start; i < stop; i++)
    {
        Crop_one(handle, input, roi_data, ivas_meta, lumaImg, chromaImg, i);
    }
    return 0;
}

static int Crop_one_bgr(
    IVASKernel *handle,
    IVASFrame *input[MAX_NUM_OBJECT],
    const ivas_ms_roi& roi_data,
    GstIvasMeta *ivas_meta,
    const Mat& bgrImg,
    int ind
)
{
    IVASFrameProps out_props = {0, };
    out_props.width = 80;
    out_props.height = 160;
    out_props.fmt = IVAS_VFMT_BGR8;
    uint32_t size = FRAME_SIZE(out_props.width, out_props.height);

    IVASFrame *out_ivas_frame = ivas_alloc_buffer(handle, size, IVAS_FRAME_MEMORY, &out_props);
    if (!out_ivas_frame)
    {
        printf("ERROR: IVAS MS: failed to allocate frame memory");
        return 0;
    }
    else
    {
        IvasObjectMetadata *ivas_obj =
            (IvasObjectMetadata *)g_list_nth_data(ivas_meta->xmeta.objects, ind);
        if (ivas_obj)
        {
            ivas_obj->obj_list = g_list_append(ivas_obj->obj_list, out_ivas_frame->app_priv);

            ivas_obj->bbox_meta.xmin = roi_data.roi[ind].x_cord;
            ivas_obj->bbox_meta.xmax = roi_data.roi[ind].x_cord + roi_data.roi[ind].width;
            ivas_obj->bbox_meta.ymin = roi_data.roi[ind].y_cord;
            ivas_obj->bbox_meta.ymax = roi_data.roi[ind].y_cord + roi_data.roi[ind].height;
            ivas_obj->obj_prob = roi_data.roi[ind].prob;
            {
                cv::Rect ROI(roi_data.roi[ind].x_cord, roi_data.roi[ind].y_cord,
                              roi_data.roi[ind].width, roi_data.roi[ind].height);

                cv::Mat subbgr = bgrImg(ROI);

                GstMapInfo info;
                gst_buffer_map((GstBuffer *)out_ivas_frame->app_priv, &info, GST_MAP_WRITE);
                char *indata = (char *)info.data;
                cv::Mat subbgrResize(out_props.height, out_props.width, CV_8UC3, indata);
                resize(subbgr, subbgrResize, subbgrResize.size());

                gst_buffer_unmap((GstBuffer *)out_ivas_frame->app_priv, &info);
            }
            out_ivas_frame->app_priv = NULL;
        }
        ivas_free_buffer(handle, out_ivas_frame);
        out_ivas_frame = NULL;
        return 0;
    }
}

static int Crop_range_bgr(
    IVASKernel *handle,
    IVASFrame *input[MAX_NUM_OBJECT],
    const ivas_ms_roi& roi_data,
    GstIvasMeta *ivas_meta,
    const Mat& bgrImg,
    int start, int stop
)
{
    cv::Mat bgrClone=bgrImg.clone();
    for (int i = start; i < stop; i++)
    {
        Crop_one_bgr(handle, input, roi_data, ivas_meta, bgrClone, i);
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

static int xlnx_multiscaler_descriptor_create (IVASKernel *handle,
    IVASFrame *input[MAX_NUM_OBJECT], IVASFrame *output[MAX_NUM_OBJECT],
    const ivas_ms_roi& roi_data)
{
    IVASFrameProps out_props = {0, };

    IVASFrame *in_ivas_frame = input[0];

    GstIvasMeta *ivas_meta = gst_buffer_get_ivas_meta((GstBuffer *)in_ivas_frame->app_priv);
    if (ivas_meta == NULL) {
        ivas_meta = gst_buffer_add_ivas_meta ((GstBuffer *)in_ivas_frame->app_priv);
        if ( ivas_meta == NULL ) {
            printf("ERROR: IVAS MS: Unable to attach metadata for resized image");
            return 0;
        }
    } else {
        printf("ERROR: IVAS MS: Not implemented with exisiting meta");
        return 0;
    }

    if (in_ivas_frame->props.fmt == IVAS_VFMT_Y_UV8_420)
    {
    LOG_MESSAGE(LOG_LEVEL_DEBUG, "Input frame is in NV12 format\n");

    Mat lumaImg(input[0]->props.height, input[0]->props.stride, CV_8UC1, (char *)in_ivas_frame->vaddr[0]);
    Mat chromaImg(input[0]->props.height / 2, input[0]->props.stride, CV_8UC1, (char *)in_ivas_frame->vaddr[1]);


    for (guint ind = 0; ind < roi_data.nobj; ind++)
    {
        IvasObjectMetadata *ivas_obj = (IvasObjectMetadata *)calloc(1, sizeof(IvasObjectMetadata));
        ivas_meta->xmeta.objects = g_list_append(ivas_meta->xmeta.objects, ivas_obj);
    }
    Thread(3, 0, roi_data.nobj, std::bind(Crop_range,
        handle,
        input,
    roi_data,
    ivas_meta,
    std::cref(lumaImg),
    std::cref(chromaImg),
    std::placeholders::_1,
    std::placeholders::_2
    ) );
    }
    else if (in_ivas_frame->props.fmt == IVAS_VFMT_BGR8)
    {
    LOG_MESSAGE(LOG_LEVEL_DEBUG, "Input frame is in BGR8 format\n");

    for (guint ind = 0; ind < roi_data.nobj; ind++)
    {
        IvasObjectMetadata *ivas_obj = (IvasObjectMetadata *)calloc(1, sizeof(IvasObjectMetadata));
        ivas_meta->xmeta.objects = g_list_append(ivas_meta->xmeta.objects, ivas_obj);
    }
    Mat bgrImg(input[0]->props.height, input[0]->props.width, CV_8UC3, (char *)in_ivas_frame->vaddr[0]);
    Crop_range_bgr(
        handle,
        input,
        roi_data,
        ivas_meta,
        bgrImg,
        0, roi_data.nobj);
    }
    else
    {
        LOG_MESSAGE(LOG_LEVEL_WARNING, "Unsupported color format %d \n", in_ivas_frame->props.fmt);
        return 0;
    }
    return 0;
}


static int parse_rect(IVASKernel * handle, int start,
      IVASFrame * input[MAX_NUM_OBJECT], IVASFrame * output[MAX_NUM_OBJECT],
      ivas_ms_roi &roi_data
      )
{
    IVASFrame *inframe = input[0];
    GstInferenceMeta *infer_meta = ((GstInferenceMeta *)gst_buffer_get_meta((GstBuffer *)
                                                              inframe->app_priv,
                                                          gst_inference_meta_api_get_type()));
    if (infer_meta == NULL)
    {
        LOG_MESSAGE(LOG_LEVEL_INFO, "ivas meta data is not available for crop");
        return false;
    }

    GstInferencePrediction *root = infer_meta->prediction;

    /* Print the entire prediction tree */
    char *pstr = gst_inference_prediction_to_string(root);
    LOG_MESSAGE(LOG_LEVEL_DEBUG, "Prediction tree: \n%s", pstr);

    roi_data.nobj = 0;
    /* Iterate through the immediate child predictions */
    for (GSList *child_predictions = gst_inference_prediction_get_children(root);
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
                roi_data.roi[ind].y_cord = (uint32_t)child->bbox.y + child->bbox.y % 2;
                roi_data.roi[ind].x_cord = (uint32_t)child->bbox.x;
                roi_data.roi[ind].height = (uint32_t)child->bbox.height - child->bbox.height % 2;
                roi_data.roi[ind].width = (uint32_t)child->bbox.width - child->bbox.width % 2;
                roi_data.roi[ind].prob = classification->class_prob;
                roi_data.nobj++;
            }
        }
    }
    return 0;
}
extern "C"
{


int32_t xlnx_kernel_start (IVASKernel *handle, int start /*unused */,
        IVASFrame *input[MAX_NUM_OBJECT], IVASFrame *output[MAX_NUM_OBJECT])
{
    int ret;
    uint32_t value = 0;
    ivas_ms_roi roi_data;
    parse_rect(handle, start, input, output, roi_data);
   /* set descriptor */
    xlnx_multiscaler_descriptor_create (handle, input, output, roi_data);

    return 0;
}

int32_t xlnx_kernel_init (IVASKernel *handle)
{
    handle->is_multiprocess = 1;        
    return 0;
}

uint32_t xlnx_kernel_deinit (IVASKernel *handle)
{
    return 0;
}

int32_t xlnx_kernel_done(IVASKernel *handle)
{
    /* dummy */
    return 0;
}

}
