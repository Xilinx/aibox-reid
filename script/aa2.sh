#
# Copyright 2021 Xilinx Inc.
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

(
log="/tmp/log.${BASHPID}"
rm ${log}
file=${1:-"/usr/share/somapp/movies/AA2/AA2-shop.nv12.30fps.1080p.h264"}
killall smartcam_aa1
smartcam_aa1 -f ${file} -t rtsp -p 5000 -n > ${log} 2>&1 &
while [ "${addr}" == "" ]; do
    addr=$(cat ${log} | grep "rtsp://")
done
aibox_aa2 -s ${addr} -t rtsp -s ${file} -t file -R
)
