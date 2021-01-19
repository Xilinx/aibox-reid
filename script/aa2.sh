#
# Copyright 2020 Xilinx Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

LD_LIBRARY_PATH="/opt/xilinx/lib/:${LD_LIBRARY_PATH}" \
gst-launch-1.0 \
filesrc location="/usr/share/somapp/movies/walking-people.nv12.30fps.1080p.h264" \
  ! h264parse ! omxh264dec internal-entropy-buffers=3 \
  ! video/x-raw, format=NV12 \
  ! tee name=t0 t0.src_0 ! queue \
    ! ivas_xm2m kconfig="/opt/xilinx/share/aibox_aa2/ped_pp.json" \
    ! ivas_xfilter name=refinedet kernels-config="/opt/xilinx/share/aibox_aa2/refinedet.json" ! queue \
    ! ivas_xfilter name=crop      kernels-config="/opt/xilinx/share/aibox_aa2/crop.json" ! queue \
    ! ivas_xfilter kernels-config="/opt/xilinx/share/aibox_aa2/reid.json" ! queue \
    ! scalem0.sink_master ivas_xmetaaffixer name=scalem0 scalem0.src_master \
    ! fakesink \
  t0.src_1 ! queue ! \
    scalem0.sink_slave_0 scalem0.src_slave_0 ! queue \
    ! ivas_xfilter kernels-config="/opt/xilinx/share/aibox_aa2/draw_reid.json" ! queue \
    ! kmssink driver-name=xlnx plane-id=39 sync=false fullscreen-overlay=true
