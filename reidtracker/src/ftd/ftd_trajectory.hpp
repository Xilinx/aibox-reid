/*
 * Copyright 2019 Xilinx Inc.
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

#ifndef _FTD_TRAJECTORY_HPP_
#define _FTD_TRAJECTORY_HPP_

#include <tuple>
#include "ftd_filter_linear.hpp"

using namespace cv;

namespace vitis {
namespace ai {

// InputCharact: roi, score, label, local_id
typedef std::tuple<cv::Mat, cv::Rect_<float>, float, int, int> InputCharact;
// OutputCharact: gid, roi, score, label, local_id
typedef std::tuple<uint64_t, cv::Rect_<float>, float, int, int> OutputCharact;

class FTD_Trajectory {
 public:
  FTD_Trajectory(SpecifiedCfg& specified_cfg);
  ~FTD_Trajectory(){};
  void Predict();
  int GetId();
  InputCharact& GetCharact();
  void Init(const InputCharact& input_charact, std::vector<uint64_t>& id_record,
            int mode);
  void UpdateTrack();
  void UpdateDetect(const InputCharact& input_charact);
  void UpdateWithoutDetect();
  void UpdateFeature(const cv::Mat& feat);
  int GetStatus();
  bool GetShown();
  OutputCharact GetOut();
  std::vector<cv::Mat> GetFeatures();
  int B2G;
  int G2B;
  int B2D;
  int leap;
  int hit_streak = 0;
  int age = 0;
  int time_since_update = 0;

 private:
  int id;
  InputCharact charact;
  // FTD_Filter filter;
  // FTD_Filter_Light filter;
  FTD_Filter_Linear filter;
  // FTD_Filter_Run filter;
  SpecifiedCfg specified_cfg_;
  std::vector<cv::Mat> features;
  cv::Mat feature;
  int status;
  bool have_been_shown;
};

}  // namespace ai
}  // namespace vitis
#endif
