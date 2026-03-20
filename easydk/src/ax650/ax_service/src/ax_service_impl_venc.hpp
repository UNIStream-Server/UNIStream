/*************************************************************************
 * Copyright (C) [2022] by Cambricon, Inc. All rights reserved
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
#ifndef MPS_SERVICE_IMPL_VENC_HPP_
#define MPS_SERVICE_IMPL_VENC_HPP_

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <queue>

#include "glog/logging.h"

#include "axcl_venc.h"
#include "ax_buffer_tool.h"
#include "../ax_service.hpp"
//#include "mps_internal/cnsample_comm.h"

namespace uniedk {

constexpr int kMaxMpsVencPackNum = 8;
constexpr int kMaxMpsVecNum = 32;

class MpsVenc : private NonCopyable {
 public:
  explicit MpsVenc(IVBInfo *vb_info) {}
  ~MpsVenc() {}

  int Config(const MpsServiceConfig &config);
  void *Create(IVEncResult *result, VencCreateParam *params);
  int Destroy(void *handle);
  int SendFrame(void *handle, const AX_VIDEO_FRAME_INFO_T *pst_frame, AX_S32 milli_sec);

 private:
  int GetId() {
    std::unique_lock<std::mutex> lk(id_mutex_);
    if (id_q_.size()) {
      int id = id_q_.front();
      id_q_.pop();
      return id + 1;
    }
    LOG(ERROR) << "[EasyDK] [MpsVenc] GetId(): No available encoder id";
    return -1;
  }
  void ReturnId(int id) {
    if (id < 1 + mps_config_.codec_id_start || id >= kMaxMpsVecNum) {
      LOG(ERROR) << "[EasyDK] [MpsVenc] ReturnId(): encoder id is invalid";
      return;
    }
    std::unique_lock<std::mutex> lk(id_mutex_);
    id_q_.push(id - 1);
  }
  int CheckHandleEos(void *handle);
  void OnFrameBits(void *handle, AX_VENC_STREAM_T *pst_stream);

 private:
  MpsServiceConfig mps_config_;
  std::mutex id_mutex_;
  std::queue<int> id_q_;

  struct VEncCtx {
    IVEncResult *result_ = nullptr;
    std::atomic<bool> eos_sent_{false};
    std::atomic<bool> error_flag_{false};
    std::atomic<bool> created_{false};
    AX_VENC_CHN_ATTR_T chn_attr_;
    AX_VENC_RECV_PIC_PARAM_T chn_param_;
    VENC_CHN venc_chn_ = -1;
	VENC_GRP grp_Id_ = -1;
	AX_VOID *host_mem_ = nullptr;
    std::mutex mutex_;
  } venc_ctx[kMaxMpsVecNum];
};

}  // namespace uniedk

#endif  // MPS_SERVICE_IMPL_VENC_HPP_
