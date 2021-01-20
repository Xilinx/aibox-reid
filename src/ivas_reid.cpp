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

#include <gst/ivas/gstivasmeta.h>
#include <ivas/ivas_kernel.h>
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

#define MAX_REID 20
#define DEFAULT_REID_THRESHOLD 0.2

using namespace std;

struct _Face {
  int last_frame_seen;
  int xctr;
  int yctr;
  int id;
  cv::Mat features;
};

typedef struct _kern_priv {
  uint32_t top;
  double threshold;
  std::shared_ptr<vitis::ai::ReidTracker> tracker;
} ReidKernelPriv;

static int ivas_reid_run(const cv::Mat &image, IVASKernel *handle,
                         int frame_num, int buf_num, int xctr, int yctr,
                         int xmin, int xmax, int ymin, int ymax, double prob) {
  int id = 0;
  return id;
}

extern "C" {
int32_t xlnx_kernel_init(IVASKernel *handle) {
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
  kernel_priv->tracker = vitis::ai::ReidTracker::create();

  handle->kernel_priv = (void *)kernel_priv;
  return 0;
}

uint32_t xlnx_kernel_deinit(IVASKernel *handle) {
  ReidKernelPriv *kernel_priv = (ReidKernelPriv *)handle->kernel_priv;
  free(kernel_priv);
  return 0;
}

int32_t xlnx_kernel_start(IVASKernel *handle, int start /*unused */,
                          IVASFrame *input[MAX_NUM_OBJECT],
                          IVASFrame *output[MAX_NUM_OBJECT]) {
  IVASFrame *in_ivas_frame = input[0];
  ReidKernelPriv *kernel_priv = (ReidKernelPriv *)handle->kernel_priv;
  static int frame_num = 0;
  frame_num++;
  std::vector<vitis::ai::ReidTracker::InputCharact> input_characts;
  /* get metadata from input */
  GstIvasMeta *ivas_meta =
      gst_buffer_get_ivas_meta((GstBuffer *)in_ivas_frame->app_priv);
  if (ivas_meta == NULL) {
    printf("meta is null, exit");
    return 0;
  } else if (g_list_length(ivas_meta->xmeta.objects) > MAX_NUM_OBJECT) {
    printf("Can't process more then %d objects", MAX_NUM_OBJECT);
    return -1;
  }

  uint32_t n_obj = ivas_meta ? g_list_length(ivas_meta->xmeta.objects) : 0;
  m__TIC__(getinput);
  for (uint32_t i = 0; i < n_obj; i++) {
    IvasObjectMetadata *xva_obj =
        (IvasObjectMetadata *)g_list_nth_data(ivas_meta->xmeta.objects, i);

    if (!xva_obj) {
      printf("ERROR: IVAS REID: Unable to get meta data pointer");
      return -1;
    } else {
      GstBuffer *buffer = (GstBuffer *)g_list_nth_data(
          xva_obj->obj_list, 0); /* resized crop image*/
      int xctr = xva_obj->bbox_meta.xmax -
                 ((xva_obj->bbox_meta.xmax - xva_obj->bbox_meta.xmin) / 2);
      int yctr = xva_obj->bbox_meta.ymax -
                 ((xva_obj->bbox_meta.ymax - xva_obj->bbox_meta.ymin) / 2);
      GstMapInfo info;
      gst_buffer_map(buffer, &info, GST_MAP_READ);

      GstVideoMeta *vmeta = gst_buffer_get_video_meta(buffer);
      if (!vmeta) {
        printf("ERROR: IVAS REID: video meta not present in buffer");
      } else if (vmeta->width == 80 && vmeta->height == 160) {
        // printf("INFO %d-%d: %f, %f, %f, %f, %f\n", frame_num, i,
        //       xva_obj->bbox_meta.xmax, xva_obj->bbox_meta.ymax,
        //       xva_obj->bbox_meta.xmin, xva_obj->bbox_meta.ymin,
        //       xva_obj->obj_prob);
        char *indata = (char *)info.data;
        cv::Mat image(vmeta->height, vmeta->width, CV_8UC3, indata);
        // TODO:
        auto input_box =
            cv::Rect2f(xva_obj->bbox_meta.xmin, xva_obj->bbox_meta.ymin,
                       xva_obj->bbox_meta.xmax - xva_obj->bbox_meta.xmin,
                       xva_obj->bbox_meta.ymax - xva_obj->bbox_meta.ymin);
        input_characts.emplace_back(input_box, xva_obj->obj_prob, -1, i,
                                    image.clone());
        // xva_obj->obj_id = ivas_reid_run(
        //    image, handle, frame_num, i, xctr, yctr, xva_obj->bbox_meta.xmin,
        //    xva_obj->bbox_meta.xmax, xva_obj->bbox_meta.ymin,
        //    xva_obj->bbox_meta.ymax, xva_obj->obj_prob);
      } else {
        printf("ERROR: IVAS REID: Invalid resolution for reid (%u x %u)\n",
               vmeta->width, vmeta->height);
      }
      gst_buffer_unmap(buffer, &info);
    }
  }
  m__TOC__(getinput);
  m__TIC__(Track);
  std::vector<vitis::ai::ReidTracker::OutputCharact> track_results =
      std::vector<vitis::ai::ReidTracker::OutputCharact>(
          kernel_priv->tracker->track(frame_num, input_characts, true, true));
  m__TOC__(Track);
  cout << "tracker result: " << endl;
  int i = 0;
  for (auto &r : track_results) {
    auto box = get<1>(r);
    float x = box.x;
    float y = box.y;
    float xmin = x;
    float ymin = y;
    float xmax = x + (box.width);
    float ymax = y + (box.height);
    uint64_t gid = get<0>(r);
    // float score = get<2>(r);
    cout << frame_num << "," << gid << "," << xmin << "," << ymin << ","
         << xmax - xmin << "," << ymax - ymin << "\n";

    IvasObjectMetadata *xva_obj =
        (IvasObjectMetadata *)g_list_nth_data(ivas_meta->xmeta.objects, i);
    xva_obj->bbox_meta.xmin = x;
    xva_obj->bbox_meta.ymin = y;
    xva_obj->bbox_meta.xmax = xmax;
    xva_obj->bbox_meta.ymax = ymax;
    xva_obj->obj_id = gid;
    i++;
  }
  return 0;
}

int32_t xlnx_kernel_done(IVASKernel *handle) {
  /* dummy */
  return 0;
}
}
