/*************************************************************************
 * Copyright (C) [2022] by Cambricon, Inc. All rights reserved
 * Copyright (C) [2025] by UNIStream Team. All rights reserved 
 *  This file has been modified by UNIStream development team based on the original work from Cambricon, Inc.
 *  The original work is licensed under the Apache License, Version 2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *************************************************************************/

#include <algorithm>
#include <memory>
#include <vector>

// #define LOCAL_DEBUG_DUMP_IMAGE

#ifdef LOCAL_DEBUG_DUMP_IMAGE
#include <atomic>
#include <string>
#include "opencv2/imgproc/types_c.h"
#include "opencv2/opencv.hpp"
#endif

#include "unistream_postproc.hpp"

#define CLIP(x) ((x) < 0 ? 0 : ((x) > 1 ? 1 : (x)))

class PostprocSSDLpd : public unistream::Postproc {
 public:
  int Execute(const unistream::NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
              const std::vector<unistream::UNIFrameInfoPtr>& packages,
              const std::vector<unistream::UNIInferObjectPtr>& objects,
              const unistream::LabelStrings& labels) override;

 private:
  DECLARE_REFLEX_OBJECT_EX(PostprocSSDLpd, unistream::Postproc);
  float threshold_ = 0.1;
};

IMPLEMENT_REFLEX_OBJECT_EX(PostprocSSDLpd, unistream::Postproc);

int PostprocSSDLpd::Execute(const unistream::NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
                            const std::vector<unistream::UNIFrameInfoPtr>& packages,
                            const std::vector<unistream::UNIInferObjectPtr>& objects,
                            const unistream::LabelStrings& labels) {
  // lpd-ssd:  mutable output, 1 output
  uniedk::BufSurfWrapperPtr output = net_outputs[0].first;  // data
  if (!output->GetHostData(0)) {
    LOGE(PostprocSSDLpd) << " copy data to host first.";
    return -1;
  }
  UniedkBufSurfaceSyncForCpu(output->GetBufSurface(), -1, -1);
  infer_server::Shape pred_dims = net_outputs[0].second;

  float* preds = static_cast<float*>(output->GetHostData(0));
  int bbox_num = pred_dims[0];
  if (!bbox_num) return 0;
  int bbox_size = pred_dims[1];
  if (bbox_size != 7) return 0;

  for (int i = 0; i < bbox_num; i++) {
    float* bbox_data = preds + i * bbox_size;
    size_t batch_idx = static_cast<size_t>(bbox_data[0]);
    if (batch_idx >= packages.size()) continue;  // FIXME
    int category = static_cast<int>(bbox_data[1]);
    if (0 == category) {
      // background
      continue;
    }
    float score = bbox_data[2];
    if (score < threshold_) continue;

    auto package = packages[batch_idx];
    auto parent = objects[batch_idx];
    unistream::UNIInferObjsPtr objs_holder = nullptr;
    if (package->collection.HasValue(unistream::kUNIInferObjsTag)) {
      objs_holder = package->collection.Get<unistream::UNIInferObjsPtr>(unistream::kUNIInferObjsTag);
    }
    if (!objs_holder) {
      return -1;
    }
    auto plate_obj = std::make_shared<unistream::UNIInferObject>();
    plate_obj->parent = parent;
    plate_obj->id = std::to_string(category);
    plate_obj->score = score;
    plate_obj->bbox.x = bbox_data[3];
    plate_obj->bbox.y = bbox_data[4];
    plate_obj->bbox.w = bbox_data[5] - bbox_data[3];
    plate_obj->bbox.h = bbox_data[6] - bbox_data[4];
    plate_obj->bbox.x = CLIP(plate_obj->bbox.x);
    plate_obj->bbox.y = CLIP(plate_obj->bbox.y);
    plate_obj->bbox.w = std::min(1 - plate_obj->bbox.x, plate_obj->bbox.w);
    plate_obj->bbox.h = std::min(1 - plate_obj->bbox.y, plate_obj->bbox.h);
    if (plate_obj->bbox.w <= 0.0f || plate_obj->bbox.h <= 0.0f) continue;

    plate_obj->AddExtraAttribute("Category", "Plate");
    std::lock_guard<std::mutex> lk(objs_holder->mutex_);
    objs_holder->objs_.push_back(plate_obj);

#ifdef LOCAL_DEBUG_DUMP_IMAGE
    static std::atomic<unsigned int> count{0};
    if (count < 100) {
      const auto frame = package->collection.Get<unistream::UNIDataFramePtr>(unistream::kUNIDataFrameTag);
      auto parent_bbox = unistream::GetFullFovBbox(parent.get());
      cv::Mat image = frame->ImageBGR();
      auto color = cv::Scalar(0, 0, 255);  // cv::Scalar(b, g, r);
      cv::Point top_left_p(parent_bbox.x * frame->buf_surf->GetWidth(), parent_bbox.y * frame->buf_surf->GetHeight());
      cv::Point bottom_right_p((parent_bbox.x + parent_bbox.w) * frame->buf_surf->GetWidth(),
                               (parent_bbox.y + parent_bbox.h) * frame->buf_surf->GetHeight());
      cv::rectangle(image, top_left_p, bottom_right_p, color, 2);
      auto obj_bbox = unistream::GetFullFovBbox(plate_obj.get());
      cv::Point top_left(obj_bbox.x * frame->buf_surf->GetWidth(), obj_bbox.y * frame->buf_surf->GetHeight());
      cv::Point bottom_right((obj_bbox.x + obj_bbox.w) * frame->buf_surf->GetWidth(),
                             (obj_bbox.y + obj_bbox.h) * frame->buf_surf->GetHeight());
      cv::rectangle(image, top_left, bottom_right, color, 2);
      std::string filename("test_ssd_lpd_bgr_");
      filename += std::to_string(count) + ".jpg";
      cv::imwrite(filename, image);
    }
    ++count;
#endif
  }
  return 0;
}
