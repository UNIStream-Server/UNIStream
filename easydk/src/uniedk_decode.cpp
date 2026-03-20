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

#include "uniedk_decode.h"

#include <cstring>  // for memset
#include <memory>  // for unique_ptr
#include <mutex>   // for call_once

#include "glog/logging.h"

#include "uniedk_decode_impl.hpp"
#include "uniedk_platform.h"
#include "common/utils.hpp"

#ifdef PLATFORM_AX650N
#include "ax650/uniedk_decode_impl_ax650.hpp"
#endif

namespace uniedk {

IDecoder *CreateDecoder() {
  int dev_id = 0;

  UniedkPlatformInfo info;
  if (UniedkPlatformGetInfo(dev_id, &info) < 0) {
    LOG(ERROR) << "[EasyDK] CreateDecoder(): Get platform information failed";
    return nullptr;
  }

// FIXME,
//  1. check prop_name ???
//  2. load so ???
  if (info.support_unified_addr) {
    return new DecoderAX650();
  }

  return nullptr;
}

class DecodeService {
 public:
  static DecodeService &Instance() {
    static std::once_flag s_flag;
    std::call_once(s_flag, [&] { instance_.reset(new DecodeService); });
    return *instance_;
  }

  int Create(void **vdec, UniedkVdecCreateParams *params) {
    if (!vdec || !params) {
      LOG(ERROR) << "[EasyDK] [DecodeService] Create(): decoder or params pointer is invalid";
      return -1;
    }
    if (CheckParams(params) < 0) {
      LOG(ERROR) << "[EasyDK] [DecodeService] Create(): Parameters are invalid";
      return -1;
    }
    IDecoder *decoder_ = CreateDecoder();
    if (!decoder_) {
      LOG(ERROR) << "[EasyDK] [DecodeService] Create(): new decoder failed";
      return -1;
    }
    if (decoder_->Create(params) < 0) {
      LOG(ERROR) << "[EasyDK] [DecodeService] Create(): Create decoder failed";
      delete decoder_;
      return -1;
    }
    *vdec = decoder_;
    return 0;
  }

  int Destroy(void *vdec) {
    if (!vdec) {
      LOG(ERROR) << "[EasyDK] [DecodeService] Destroy(): Decoder pointer is invalid";
      return -1;
    }
    IDecoder *decoder_ = static_cast<IDecoder *>(vdec);
    decoder_->Destroy();
    delete decoder_;
    return 0;
  }

  int SendStream(void *vdec, const UniedkVdecStream *stream, int timeout_ms) {
    if (!vdec || !stream) {
      LOG(ERROR) << "[EasyDK] [DecodeService] SendStream(): Decoder or stream pointer is invalid";
      return -1;
    }
    IDecoder *decoder_ = static_cast<IDecoder *>(vdec);
    return decoder_->SendStream(stream, timeout_ms);
  }

 private:
  int CheckParams(UniedkVdecCreateParams *params) {
    if (params->type <= UNIEDK_VDEC_TYPE_INVALID || params->type >= UNIEDK_VDEC_TYPE_NUM) {
      LOG(ERROR) << "[EasyDK] [DecodeService] CheckParams(): Unsupported codec type: " << params->type;
      return -1;
    }

    if (params->color_format != UNIEDK_BUF_COLOR_FORMAT_NV12 && params->color_format != UNIEDK_BUF_COLOR_FORMAT_NV21) {
      LOG(ERROR) << "[EasyDK] [DecodeService] CheckParams(): Unsupported color format: " << params->color_format;
      return -1;
    }

    if (params->OnEos == nullptr || params->OnFrame == nullptr || params->OnError == nullptr ||
        params->GetBufSurf == nullptr) {
      LOG(ERROR) << "[EasyDK] [DecodeService] CheckParams(): OnEos, OnFrame, OnError or GetBufSurf function pointer"
                 << " is invalid";
      return -1;
    }

    return 0;
  }

 private:
  DecodeService(const DecodeService &) = delete;
  DecodeService(DecodeService &&) = delete;
  DecodeService &operator=(const DecodeService &) = delete;
  DecodeService &operator=(DecodeService &&) = delete;
  DecodeService() = default;

 private:
  static std::unique_ptr<DecodeService> instance_;
};

std::unique_ptr<DecodeService> DecodeService::instance_;

}  // namespace uniedk

extern "C" {

int UniedkVdecCreate(void **vdec, UniedkVdecCreateParams *params) {
  return uniedk::DecodeService::Instance().Create(vdec, params);
}
int UniedkVdecDestroy(void *vdec) { return uniedk::DecodeService::Instance().Destroy(vdec); }
int UniedkVdecSendStream(void *vdec, const UniedkVdecStream *stream, int timeout_ms) {
  return uniedk::DecodeService::Instance().SendStream(vdec, stream, timeout_ms);
}
};
