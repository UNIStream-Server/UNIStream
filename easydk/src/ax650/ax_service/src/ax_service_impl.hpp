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
#ifndef MPS_SERVICE_IMPL_HPP_
#define MPS_SERVICE_IMPL_HPP_

#include <map>
#include <memory>
#include <mutex>
#include "../ax_service.hpp"
#include "ax_service_impl_vdec.hpp"
#include "ax_service_impl_venc.hpp"

namespace uniedk {
class MpsServiceImpl : private NonCopyable, public IVBInfo {
 public:
  MpsServiceImpl() {
    mps_vdec_.reset(new MpsVdec(this));
    mps_venc_.reset(new MpsVenc(this));
  }
  ~MpsServiceImpl() { Destroy(); }

  int Init(const MpsServiceConfig &config);
  void Destroy();
  void OnVBInfo(AX_U64 blkSize, int blkCount) override;

  // vdecs (vdec + vpps or vdec only)
  void *CreateVDec(IVDecResult *result, AX_PAYLOAD_TYPE_E type, int max_width, int max_height, int buf_num = 12,
                   AX_IMG_FORMAT_E pix_fmt = AX_FORMAT_YUV420_SEMIPLANAR) {
    if (mps_vdec_) {
      return mps_vdec_->Create(result, type, max_width, max_height, buf_num, pix_fmt);
    }
    return nullptr;
  }
  int DestroyVDec(void *handle) {
    if (mps_vdec_) {
      return mps_vdec_->Destroy(handle);
    }
    return -1;
  }
  int VDecSendStream(void *handle, const AX_VDEC_STREAM_T *pst_stream, AX_S32 milli_sec) {
    if (mps_vdec_) {
      return mps_vdec_->SendStream(handle, pst_stream, milli_sec);
    }
    return -1;
  }

  int VDecReleaseFrame(void *handle, const AX_VIDEO_FRAME_INFO_T *info) {
    if (mps_vdec_) {
      return mps_vdec_->ReleaseFrame(handle, info);
    }
    return -1;
  }

  // vencs
  void *CreateVEnc(IVEncResult *result, VencCreateParam *params) {
    if (mps_venc_) {
      return mps_venc_->Create(result, params);
    }
    return nullptr;
  }
  int DestroyVEnc(void *handle) {
    if (mps_venc_) {
      return mps_venc_->Destroy(handle);
    }
    return -1;
  }
  int VEncSendFrame(void *handle, const AX_VIDEO_FRAME_INFO_T *pst_frame, AX_S32 milli_sec) {
    if (mps_venc_) {
      return mps_venc_->SendFrame(handle, pst_frame, milli_sec);
    }
    return -1;
  }

 private:
  AX_S32 InitSys();

 private:
  MpsServiceConfig mps_config_;
  std::map<AX_U64, int> pool_cfgs_;
  std::unique_ptr<MpsVdec> mps_vdec_ = nullptr;
  std::unique_ptr<MpsVenc> mps_venc_ = nullptr;
};

}  // namespace uniedk

#endif  // MPS_SERVICE_IMPL_HPP_
