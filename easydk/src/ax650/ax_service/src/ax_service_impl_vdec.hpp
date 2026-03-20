/*************************************************************************
 * Copyright (C) [2021] by Cambricon, Inc. All rights reserved
 * Copyright (C) [2025] by UNIStream Team. All rights reserved
 *
 * This file has been modified by UNIStream development team based on
 * the original work from Cambricon, Inc.
 * The original work is licensed under the Apache License, Version 2.0.
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
#ifndef AX_SERVICE_IMPL_VDEC_HPP_
#define AX_SERVICE_IMPL_VDEC_HPP_

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <queue>

#include "glog/logging.h"

#include "axcl_vdec.h"

#include "../ax_service.hpp"

namespace uniedk {

constexpr int kMaxMpsVdecNum = 32;

class MpsVdec : private NonCopyable {
 public:
  explicit MpsVdec(IVBInfo *vb_info) : vb_info_(vb_info) {}
  ~MpsVdec() {}

  int Config(const MpsServiceConfig &config);
  void *Create(IVDecResult *result, AX_PAYLOAD_TYPE_E type, int max_width, int max_height, int buf_num = 12,
               AX_IMG_FORMAT_E pix_fmt = AX_FORMAT_YUV420_SEMIPLANAR);
  int Destroy(void *handle);
  int SendStream(void *handle, const AX_VDEC_STREAM_T *pst_stream, AX_S32 milli_sec);
  int ReleaseFrame(void *handle, const AX_VIDEO_FRAME_INFO_T *info);

 private:
  int GetId() {
    std::unique_lock<std::mutex> lk(id_mutex_);
    if (id_q_.size()) {
      int id = id_q_.front();
      id_q_.pop();
      return id + 1;
    }
    LOG(ERROR) << "[EasyDK] [MpsVdec] GetId(): No available decoder id";
    return -1;
  }
  void ReturnId(int id) {
    std::unique_lock<std::mutex> lk(id_mutex_);
    if (id < mps_config_.codec_id_start + 1 || id >= kMaxMpsVdecNum) {
      LOG(ERROR) << "[EasyDK] [MpsVdec] ReturnId(): decoder id is invalid";
      return;
    }
    id_q_.push(id - 1);
  }
  int CheckHandleEos(void *handle);

 private:
  IVBInfo *vb_info_ = nullptr;
  MpsServiceConfig mps_config_;
  std::mutex id_mutex_;
  std::queue<int> id_q_;

  struct VDecCtx {
    IVDecResult *result_ = nullptr;
    std::atomic<bool> eos_sent_{false};
    std::atomic<bool> error_flag_{false};
    std::atomic<bool> created_{false};
	AX_VDEC_GRP_PARAM_T grp_param_;
	AX_VDEC_GRP_ATTR_T grp_attr_;
	AX_VDEC_CHN_ATTR_T chn_attr_;
    AX_VDEC_GRP vdec_chn_ = -1;
    std::mutex mutex_;
  } vdec_ctx_[kMaxMpsVdecNum];
};

}  // namespace uniedk

#endif  // AX_SERVICE_IMPL_VDEC_HPP_
