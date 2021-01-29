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

#include "ftd_structure.hpp"
#include <glog/logging.h>
#include "../common.hpp"

using namespace cv;
using namespace std;

namespace vitis {
namespace ai {

FTD_Structure::FTD_Structure(const SpecifiedCfg& specified_cfg) {
  CHECK(id_record.empty()) << "id_record must be empty when initial";
  id_record.push_back(1);
  iou_threshold = 0.5f;
  feat_distance_low = 0.8f;
  feat_distance_high = 1.0f;
  score_threshold = 0.f;
  specified_cfg_ = specified_cfg;
  reid = vitis::ai::Reid::create("reid");
}

FTD_Structure::~FTD_Structure() { this->clear(); }

void FTD_Structure::clear() {
  tracks.clear();
  id_record.clear();
  remove_id_this_frame.clear();
  id_record.push_back(1);
}

double cosine_distance(Mat feat1, Mat feat2) { return 1 - feat1.dot(feat2); }

double get_euro_dis(Mat feat1, Mat feat2) {
  CHECK(feat1.cols == feat2.cols)
      << "error happen in get_euro_dis" << feat1.cols << " vs. " << feat2.cols;
  double sumvalue = 0;
  for (int i = 0; i < feat1.cols; i++) {
    sumvalue += (feat1.at<float>(0, i) - feat2.at<float>(0, i)) *
                (feat1.at<float>(0, i) - feat2.at<float>(0, i));
  }
  return sqrt(sumvalue);
}

void FindRemain(std::vector<int>& input, std::vector<int>& output, int len) {
  output.clear();
  for (int i = 0; i < len; i++) {
    if (input.empty() ||
        std::find(input.begin(), input.end(), i) == input.end()) {
      output.push_back(i);
    }
  }
  CHECK((int)(input.size() + output.size()) == len)
      << "error happen in function FindRemain";
}

float GetIou(const cv::Rect_<float>& rect1, const cv::Rect_<float>& rect2) {
  float inner = (rect1 & rect2).area();
  float univer = rect1.area() + rect2.area() - inner;
  return (inner / univer);
}
float GetCenterDis(const cv::Rect_<float>& rect1,
                   const cv::Rect_<float>& rect2) {
  float cx = rect1.x + rect1.width * 0.5;
  float cy = rect1.y + rect1.height * 0.5;
  if (cx >= rect2.x && cx <= rect2.x + rect2.width && cy >= rect2.y &&
      cy <= rect2.y + rect2.height) {
    return 1;
  } else
    return 0;
}
float GetCoverRatio(const cv::Rect_<float>& rect1,
                    const cv::Rect_<float>& rect2) {
  float inner = (rect1 & rect2).area();
  return (inner / rect1.area());
}

void FTD_Structure::GetOut(std::vector<OutputCharact>& output_characts) {
  CHECK(output_characts.size() == 0) << "error output_characts size";
  for (auto ti = tracks.end() - 1; ti >= tracks.begin();) {
    if ((((*ti)->time_since_update) < 1) &&
        (((*ti)->hit_streak >= min_hits) || frame_count <= min_hits)) {
      auto oout = (*ti)->GetOut();
      std::get<1>(oout) = (std::get<1>(oout));
      output_characts.push_back(oout);
    }
    if ((*ti)->time_since_update > max_age) {
      ti = tracks.erase(ti);
    }
    ti--;
  }
}

std::vector<OutputCharact> FTD_Structure::Update(
    uint64_t frame_id, bool detect_flag, int mode,
    std::vector<InputCharact>& input_characts) {
  remove_id_this_frame.clear();
  frame_count += 1;
  LOG_IF(INFO, ENV_PARAM(DEBUG_REID_TRACKER))
      << "frame " << frame_id << " detect_flag " << detect_flag;
  // get range of frame and check detect_flag
  std::vector<OutputCharact> output_characts;
  if (detect_flag == false)
    CHECK(input_characts.size() == 0) << "error input_characts size";
  // show and prune predict
  LOG_IF(INFO, ENV_PARAM(DEBUG_REID_TRACKER))
      << "there are already " << tracks.size()
      << " trajectory(id predict_bbox):";
  for (auto ti = tracks.begin(); ti != tracks.end();) {
    (*ti)->Predict();
    auto track_rect = std::get<0>((*ti)->GetCharact());
    if (track_rect.width <= 0.f || track_rect.height <= 0.f) {
      auto track_id = (*ti)->GetId();
      LOG_IF(INFO, ENV_PARAM(DEBUG_REID_TRACKER))
          << "trajectory " << track_id << " predict fail, remove " << track_id;
      ti = tracks.erase(ti);
    } else {
      auto track_id = (*ti)->GetId();
      LOG_IF(INFO, ENV_PARAM(DEBUG_REID_TRACKER))
          << track_id << " " << track_rect;
      ti++;
    }
  }
  if (detect_flag == false) {
    for (auto ti : tracks) ti->UpdateWithoutDetect();
    GetOut(output_characts);
    return output_characts;
  }
  // show detect
  LOG_IF(INFO, ENV_PARAM(DEBUG_REID_TRACKER))
      << "there are " << input_characts.size() << " new detections(bbox):";
  LOG_IF(INFO, ENV_PARAM(DEBUG_REID_TRACKER)) << "size: " << tracks.size();
  for (auto ici = input_characts.begin(); ici != input_characts.end();) {
    auto rect = std::get<0>(*ici);
    auto ici_score = std::get<1>(*ici);
    if (rect.width <= 0.f || rect.height <= 0.f ||
        ici_score < score_threshold) {
      LOG_IF(INFO, ENV_PARAM(DEBUG_REID_TRACKER))
          << "erase rect : " << rect << " score: " << ici_score;
      ici = input_characts.erase(ici);
    } else {
      std::get<0>(*ici) = rect;
      LOG_IF(INFO, ENV_PARAM(DEBUG_REID_TRACKER)) << rect;
      ici++;
    }
  }
  LOG_IF(INFO, ENV_PARAM(DEBUG_REID_TRACKER))
      << "size: " << input_characts.size();
  __TIC__(get_feats);
  vector<cv::Mat> detect_imgs;
  for (auto& ic : input_characts) {
    auto img = std::get<4>(ic);
    detect_imgs.push_back(img);
  }

  __TOC__(get_feats);
  __TIC__(deal);
  /*cal iou between predict and det*/
  vector<vector<double>> neg_iou_scores;
  vector<vector<double>> center_dists;
  for (auto& t : tracks) {
    std::vector<double> neg_iou_score;
    std::vector<double> center_dis;
    for (auto& ic : input_characts) {
      auto rect_t = std::get<0>(t->GetCharact());
      auto label_t = std::get<2>(t->GetCharact());
      auto rect_i = std::get<0>(ic);
      auto label_i = std::get<2>(ic);
      neg_iou_score.push_back(
          label_t == label_i ? (1.0f - GetIou(rect_t, rect_i)) : 1.0f);
      center_dis.push_back(label_t == label_i ? GetCenterDis(rect_i, rect_t)
                                              : 0.0f);
    }
    neg_iou_scores.emplace_back(neg_iou_score);
    center_dists.emplace_back(center_dis);
  }
  // CHECK SIZE for all matrix
  int divide1 = tracks.size();
  int divide2 = input_characts.size();
  CHECK((int)neg_iou_scores.size() == divide1) << "iou_score size error";

  FtdHungarian HungAlgo;
  vector<int> assignment;
  HungAlgo.Solve(neg_iou_scores, assignment);
  LOG_IF(INFO, ENV_PARAM(DEBUG_REID_TRACKER))
      << "assign size: " << assignment.size();
  LOG_IF(INFO, ENV_PARAM(DEBUG_REID_TRACKER)) << "iou: ";
  for (unsigned int x = 0; x < assignment.size(); x++)
    LOG_IF(INFO, ENV_PARAM(DEBUG_REID_TRACKER)) << x << " " << assignment[x];
  // greedy find match track and detect number
  std::vector<int> match_track;
  std::vector<int> match_detect;
  for (size_t iii = 0; iii < assignment.size(); iii++) {
    if (assignment[iii] == -1) continue;
    if (((1.0f - neg_iou_scores[iii][assignment[iii]]) >= iou_threshold) &&
        assignment[iii] != -1) {
      match_track.push_back(iii);
      match_detect.push_back(assignment[iii]);
    }
  }
  CHECK(match_track.size() == match_detect.size())
      << "match_track and match_detect must have the same size";

  // find unmatch_track and unmatch_detect number by order
  vector<int> unmatch_track;
  FindRemain(match_track, unmatch_track, divide1);
  vector<int> unmatch_detect;
  FindRemain(match_detect, unmatch_detect, divide2);
  // double dismat[features.size()][feats.size()];
  vector<cv::Mat> feats, all_feats;
  map<int, int> feat_unmatch;
  __TIC__(allreid);
  for (auto i = 0u; i < unmatch_detect.size(); ++i) {
    __TIC__(reid);
//    cout << detect_imgs[unmatch_detect[i]].size() << endl;
    Mat feat = reid->run(detect_imgs[unmatch_detect[i]]).feat;
    __TOC__(reid);
    feats.emplace_back(feat);
    feat_unmatch[unmatch_detect[i]] = i;
  }
  if (frame_id % 5 == 0) {
    for (auto i = 0u; i < detect_imgs.size(); ++i) {
      Mat feat = reid->run(detect_imgs[i]).feat;
      all_feats.emplace_back(feat);
    }
  }
  __TOC__(allreid);
  CHECK(feats.size() == unmatch_detect.size())
      << "feats size and match_detect must have the same size";
  vector<vector<double>> feat_dists;
  if (!unmatch_track.empty() && !feats.empty()) {
    feat_dists = vector<vector<double>>(unmatch_track.size(),
                                        vector<double>(feats.size(), 0));
    for (size_t i = 0; i < unmatch_track.size(); ++i) {
      for (size_t j = 0; j < feats.size(); ++j) {
        double min_dis = 2.0;
        for (size_t h = 0; h < tracks[i]->GetFeatures().size(); ++h) {
          // for (size_t h = 0; h < 1; ++h) {
          double cdis = get_euro_dis(tracks[unmatch_track[i]]->GetFeatures()[h],
                                     feats[j]);
          LOG_IF(INFO, ENV_PARAM(DEBUG_REID_TRACKER)) << "cdis " << cdis;
          // double cdis = cosine_distance(tracks[i]->GetFeatures()[h],
          min_dis = cdis < min_dis ? cdis : min_dis;
        }
        feat_dists[i][j] = min_dis;
      }
    }
  }
  LOG_IF(INFO, ENV_PARAM(DEBUG_REID_TRACKER)) << "bus";
  // for (unsigned int x = 0; x < feat_dists.size(); x++) {
  //  for (unsigned int y = 0; y < feat_dists[x].size(); y++) {
  //    cout << feat_dists[x][y] << " ";
  //  }
  //  cout << endl;
  //}

  vector<int> feats_assign;
  HungAlgo.Solve(feat_dists, feats_assign);
  LOG_IF(INFO, ENV_PARAM(DEBUG_REID_TRACKER)) << "feats: ";
  for (unsigned int x = 0; x < feats_assign.size(); x++)
    LOG_IF(INFO, ENV_PARAM(DEBUG_REID_TRACKER)) << x << " " << feats_assign[x];
  for (size_t iii = 0; iii < feats_assign.size(); iii++) {
    if (feats_assign[iii] == -1) continue;
    if (feat_dists[iii][feats_assign[iii]] < feat_distance_low) {
      match_track.push_back(unmatch_track[iii]);
      match_detect.push_back(unmatch_detect[feats_assign[iii]]);
      for (auto i = 0u; i < feat_dists.size(); i++) {
        feat_dists[i][feats_assign[iii]] = feat_distance_high + 1.0f;
      }
      for (auto i = 0u; i < feat_dists[0].size(); i++) {
        feat_dists[iii][i] = feat_distance_high + 1.0f;
      }
    }
  }

  if (!unmatch_track.empty() && !unmatch_detect.empty()) {
    for (size_t i = 0; i < feat_dists.size(); ++i) {
      for (size_t j = 0; j < feat_dists[i].size(); ++j) {
        if (!center_dists[unmatch_track[i]][unmatch_detect[j]])
          feat_dists[i][j] = feat_distance_high + 1.0f;
      }
    }
    feats_assign.clear();
    HungAlgo.Solve(feat_dists, feats_assign);
    LOG_IF(INFO, ENV_PARAM(DEBUG_REID_TRACKER)) << "feats2: ";
    for (unsigned int x = 0; x < feats_assign.size(); x++)
      LOG_IF(INFO, ENV_PARAM(DEBUG_REID_TRACKER))
          << x << " " << feats_assign[x];
    for (size_t iii = 0; iii < feats_assign.size(); iii++) {
      if (feats_assign[iii] == -1) continue;
      if (feat_dists[iii][feats_assign[iii]] < feat_distance_high) {
        match_track.push_back(unmatch_track[iii]);
        match_detect.push_back(unmatch_detect[feats_assign[iii]]);
      }
    }
    // find unmatch_track and unmatch_detect number by order
    unmatch_track.clear();
    FindRemain(match_track, unmatch_track, divide1);
    unmatch_detect.clear();
    FindRemain(match_detect, unmatch_detect, divide2);
  }
  /*new detection with new id*/
  for (unsigned int i = 0; i < unmatch_detect.size(); i++) {
    // reid for new detection here
    tracks.push_back(std::make_shared<FTD_Trajectory>(specified_cfg_));
    tracks.back()->Init(input_characts[unmatch_detect[i]], id_record, mode);
    tracks.back()->UpdateFeature(feats[feat_unmatch[unmatch_detect[i]]]);
  }

  /*strategy for match_track detect and unmatch detect*/
  for (unsigned int i = 0; i < match_track.size(); i++) {
    tracks[match_track[i]]->UpdateDetect(input_characts[match_detect[i]]);
    if (frame_id % 5 == 0) {
      tracks[match_track[i]]->UpdateFeature(all_feats[match_detect[i]]);
    }
  }
  GetOut(output_characts);
  __TOC__(deal);
  return output_characts;
}  // namespace ai

std::vector<int> FTD_Structure::GetRemoveID() { return remove_id_this_frame; }

}  // namespace ai
}  // namespace vitis
