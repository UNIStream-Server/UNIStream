/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
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

#ifndef FEATURE_EXTRACTOR_HPP_
#define FEATURE_EXTRACTOR_HPP_
#include <opencv2/core/core.hpp>

#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "uniis/processor.h"
#include "unistream_frame.hpp"
#include "unistream_frame_va.hpp"
#include "unistream_preproc.hpp"
#include "util/unistream_queue.hpp"

namespace unistream {

using UNIInferObjectPtr = std::shared_ptr<UNIInferObject>;
using InferVideoPixelFmt = infer_server::NetworkInputFormat;

class FeatureExtractor : public infer_server::IPreproc, public infer_server::IPostproc {
 public:
  explicit FeatureExtractor(std::function<void(const UNIFrameInfoPtr, bool)> callback) : callback_(callback) {}
  FeatureExtractor(const std::shared_ptr<infer_server::ModelInfo>& model,
                   std::function<void(const UNIFrameInfoPtr, bool)> callback, int device_id = 0);
  ~FeatureExtractor();

  bool Init(InferVideoPixelFmt model_input_format, int engine_num, uint32_t batch_timeout = 3000,
            int priority = 0);
  /*******************************************************
   * @brief inference and extract features of objects
   * @param
   *   frame[in] full frame info
   * @return true if success, otherwise false
   * *****************************************************/
  bool ExtractFeature(const UNIFrameInfoPtr& info);

  void WaitTaskDone(const std::string& stream_id);

 private:
  int OnTensorParams(const infer_server::UniPreprocTensorParams* params) override;
  int OnPreproc(uniedk::BufSurfWrapperPtr src, uniedk::BufSurfWrapperPtr dst,
                const std::vector<UniedkTransformRect>& src_rects) override;
  int OnPostproc(const std::vector<infer_server::InferData*>& data_vec,
                 const infer_server::ModelIO& model_output,
                 const infer_server::ModelInfo* model_info) override;

 private:
  bool ExtractFeatureOnMlu(const UNIFrameInfoPtr& info);
  bool ExtractFeatureOnCpu(const UNIFrameInfoPtr& info);
  float CalcFeatureOfRow(const cv::Mat& image, int n);

  std::shared_ptr<infer_server::ModelInfo> model_{nullptr};
  std::unique_ptr<infer_server::InferServer> server_{nullptr};
  infer_server::Session_t session_{nullptr};
  std::function<void(const UNIFrameInfoPtr, bool)> callback_{nullptr};
  int device_id_;
  bool is_initialized_ = false;
  UniPreprocNetworkInfo info_;
  std::vector<float> mean_{0.485, 0.456, 0.406};
  std::vector<float> std_{0.229, 0.224, 0.225};
};  // class FeatureExtractor

}  // namespace unistream

#endif  // FEATURE_EXTRACTOR_HPP_
