/*************************************************************************
 * Copyright (C) [2025] by UNIStream Team. All rights reserved 
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

#include "../ax_service.hpp"

#include <map>
#include <mutex>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "axcl_native.h"
#include "glog/logging.h"

#include "ax_service_impl.hpp"

namespace uniedk {

MpsService::MpsService() { impl_.reset(new MpsServiceImpl()); }

MpsService::~MpsService() {
  if (impl_) {
    impl_->Destroy();
  }
}

int MpsService::Init(const MpsServiceConfig &config) {
  if (impl_) {
    return impl_->Init(config);
  }
  return -1;
}

void MpsService::Destroy() {
  if (impl_) {
    impl_->Destroy();
    impl_.reset();
    impl_ = nullptr;
  }
}

// vdecs (vdec + vpps or vdec only)
void *MpsService::CreateVDec(IVDecResult *result, AX_PAYLOAD_TYPE_E type, int max_width, int max_height, int buf_num,
                             AX_IMG_FORMAT_E pix_fmt) {
  if (impl_) {
    return impl_->CreateVDec(result, type, max_width, max_height, buf_num, pix_fmt);
  }
  return nullptr;
}
int MpsService::DestroyVDec(void *handle) {
  if (impl_) {
    return impl_->DestroyVDec(handle);
  }
  return -1;
}
int MpsService::VDecSendStream(void *handle, const AX_VDEC_STREAM_T *pst_stream, AX_S32 milli_sec) {
  if (impl_) {
    return impl_->VDecSendStream(handle, pst_stream, milli_sec);
  }
  return -1;
}

int MpsService::VDecReleaseFrame(void *handle, const AX_VIDEO_FRAME_INFO_T *info) {
  if (impl_) {
    return impl_->VDecReleaseFrame(handle, info);
  }
  return -1;
}

// vencs
void *MpsService::CreateVEnc(IVEncResult *result, VencCreateParam *params) {
  if (impl_) {
    return impl_->CreateVEnc(result, params);
  }
  return nullptr;
}
int MpsService::DestroyVEnc(void *handle) {
  if (impl_) {
    return impl_->DestroyVEnc(handle);
  }
  return -1;
}

int MpsService::VEncSendFrame(void *handle, const AX_VIDEO_FRAME_INFO_T *pst_frame, AX_S32 milli_sec) {
  if (impl_) {
    return impl_->VEncSendFrame(handle, pst_frame, milli_sec);
  }
  return -1;
}

// vgu resize-convert
//
static std::mutex vgu_mutex;  // there is only one VGU instance.
int MpsService::VguScaleCsc(const AX_VIDEO_FRAME_INFO_T *src, AX_VIDEO_FRAME_INFO_T *dst) {
  if (!src || !dst) {
    return -1;
  }

  int ret = 0;

  AX_IVPS_ASPECT_RATIO_T tAspectRatio;
  tAspectRatio.eMode = AX_IVPS_ASPECT_RATIO_STRETCH;
  tAspectRatio.eAligns[0] = AX_IVPS_ASPECT_RATIO_HORIZONTAL_CENTER;
  tAspectRatio.eAligns[1] = AX_IVPS_ASPECT_RATIO_VERTICAL_CENTER;
  tAspectRatio.nBgColor = 0x0000FF;

  ret = AXCL_IVPS_CropResizeVpp(&src->stVFrame, &dst->stVFrame, &tAspectRatio);
  if (ret != AXCL_SUCC) {
    LOG(ERROR) << "[EasyDK] [MpsService] VguScaleCsc(): AXCL_IVPS_CropResizeVpp failed, ret = " << ret;
    return -1;
  }
  
  return ret;
}

}  // namespace uniedk
