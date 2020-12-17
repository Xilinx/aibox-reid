/*
 * Copyright 2020 Xilinx Inc.
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
#include <opencv2/opencv.hpp>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <gst/ivas/gstivasmeta.h>
#include <ivas/ivas_kernel.h>
#include <vitis/ai/nnpp/reid.hpp>
#include <vitis/ai/reid.hpp>

#define MAX_REID 20
#define DEFAULT_REID_THRESHOLD 0.2

using namespace std;

struct _Face
{
    int last_frame_seen;
    int xctr;
    int yctr;
    int id;
    cv::Mat features;
};

typedef struct _kern_priv
{
    uint32_t top;
    bool dump_frames;
    double threshold;
    std::unique_ptr<vitis::ai::Reid> det;
} ReidKernelPriv;


static int ivas_reid_run(const cv::Mat &image, IVASKernel *handle, int frame_num, int buf_num, int xctr, int yctr)
{
    int id = 0;
    return id;
}

extern "C"
{
    int32_t xlnx_kernel_init(IVASKernel *handle)
    {
        json_t *jconfig = handle->kernel_config;
        json_t *val; /* kernel config from app */

        handle->is_multiprocess     = 1;
        ReidKernelPriv *kernel_priv = (ReidKernelPriv *)calloc(1, sizeof(ReidKernelPriv));
        if (!kernel_priv) {
            printf("Error: Unable to allocate reID kernel memory\n");
        }

        /* parse config */
        val = json_object_get(jconfig, "threshold");
        if (!val || !json_is_number(val))
            kernel_priv->threshold = DEFAULT_REID_THRESHOLD;
        else
            kernel_priv->threshold = json_number_value(val);
        kernel_priv->det = vitis::ai::Reid::create("reid");
        printf("xx %p\n", kernel_priv->det.get());

        handle->kernel_priv = (void *)kernel_priv;
        return 0;
    }

    uint32_t xlnx_kernel_deinit(IVASKernel *handle)
    {
        ReidKernelPriv *kernel_priv = (ReidKernelPriv *)handle->kernel_priv;
        free(kernel_priv);
        return 0;
    }

    int32_t xlnx_kernel_start(IVASKernel *handle, int start /*unused */, IVASFrame *input[MAX_NUM_OBJECT], IVASFrame *output[MAX_NUM_OBJECT])
    {
        GstIvasMeta *ivas_meta = NULL;
        uint32_t i             = 0, n_obj;
        IVASFrame *in_ivas_frame;
        in_ivas_frame               = input[0];
        IvasObjectMetadata *xva_obj = NULL;
        GstBuffer *buffer, *buffer_crop;
        GstMapInfo info, info_crop;
        GstVideoMeta *vmeta, *vmeta_crop;
        ReidKernelPriv *kernel_priv = (ReidKernelPriv *)handle->kernel_priv;
        static int frame_num        = 0;
        int xctr, yctr;
        frame_num++;

        /* get metadata from input */
        ivas_meta = gst_buffer_get_ivas_meta((GstBuffer *)in_ivas_frame->app_priv);
        if (ivas_meta == NULL) {
            return 0;
        }
        else if (g_list_length(ivas_meta->xmeta.objects) > MAX_NUM_OBJECT) {
            printf("Can't process more then %d objects", MAX_NUM_OBJECT);
            return -1;
        }
        n_obj = ivas_meta ? g_list_length(ivas_meta->xmeta.objects) : 0;
        printf("nobj : %d\n", n_obj);
        for (i = 0; i < n_obj; i++) {
            xva_obj = (IvasObjectMetadata *)g_list_nth_data(ivas_meta->xmeta.objects, i);
            if (xva_obj) {
                buffer_crop = (GstBuffer *)g_list_nth_data(xva_obj->obj_list, 0); /* original crop */
                buffer      = (GstBuffer *)g_list_nth_data(xva_obj->obj_list, 1); /* 80x80 resized image*/
                xctr        = xva_obj->bbox_meta.xmax - ((xva_obj->bbox_meta.xmax - xva_obj->bbox_meta.xmin) / 2);
                yctr        = xva_obj->bbox_meta.ymax - ((xva_obj->bbox_meta.ymax - xva_obj->bbox_meta.ymin) / 2);
            }
            else {
                printf("ERROR: IVAS REID: Unable to get meta data pointer");
                return -1;
            }
            gst_buffer_map(buffer, &info, GST_MAP_READ);
            gst_buffer_map(buffer_crop, &info_crop, GST_MAP_READ);

            vmeta      = gst_buffer_get_video_meta(buffer);
            vmeta_crop = gst_buffer_get_video_meta(buffer_crop);
            if (!vmeta || !vmeta_crop) {
                printf("ERROR: IVAS REID: video meta not present in buffer");
            }
            else if (vmeta->width == 80 && vmeta->height == 80) {
                char *indata = (char *)info.data;
                cv::Mat image(vmeta->height, vmeta->width, CV_8UC3, indata);
// TODO:
                xva_obj->obj_id = ivas_reid_run(image, handle, frame_num, i, xctr, yctr);
            }
            else {
                printf("ERROR: IVAS REID: Invalid resolution for reid (%u x %u)\n", vmeta->width, vmeta->height);
            }
            gst_buffer_unmap(buffer, &info);
            gst_buffer_unmap(buffer, &info_crop);
        }
        return 0;
    }

    int32_t xlnx_kernel_done(IVASKernel *handle)
    {
        /* dummy */
        return 0;
    }
}
