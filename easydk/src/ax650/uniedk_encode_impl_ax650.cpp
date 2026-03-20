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

#include "uniedk_encode_impl_ax650.hpp"

#include <cstring>  // for memset
#include "glog/logging.h"

namespace uniedk {

// IEncoder
int EncoderAx650::Create(UniedkVencCreateParams *params) {
  create_params_ = *params;
  VencCreateParam create_params;
  memset(&create_params, 0, sizeof(create_params));

  if (params->type == UNIEDK_VENC_TYPE_H264) {
    create_params.type = PT_H264;
  } else if (params->type == UNIEDK_VENC_TYPE_H265) {
    create_params.type = PT_H265;
  } else if (params->type == UNIEDK_VENC_TYPE_JPEG) {
    create_params.type = PT_JPEG;
  } else {
    LOG(ERROR) << "[EasyDK] [EncoderAx650] Create(): Unsupported codec type: " << params->type;
    return -1;
  }

  create_params.bitrate = params->bitrate >> 10;
  if (create_params.bitrate < 10 || create_params.bitrate > 800000) {
    LOG(ERROR) << "[EasyDK] [EncoderAx650] Create(): bitrate is out of range(10240, 819200000), value: "
               << params->bitrate;
    return -1;
  }
  create_params.width = params->width;
  create_params.height = params->height;
  create_params.frame_rate = params->frame_rate;
  create_params.gop_size = params->gop_size;

  venc_ = MpsService::Instance().CreateVEnc(this, &create_params);
  if (venc_ == nullptr) {
    LOG(ERROR) << "[EasyDK] [EncoderAx650] Create(): Create encoder failed";
    return -1;
  }
  return 0;
}

int EncoderAx650::Destroy() {
  MpsService::Instance().DestroyVEnc(venc_);
  return 0;
}

int EncoderAx650::SendFrame(UniedkBufSurface *surf, int timeout_ms) {
  if (!surf || surf->surface_list[0].data_ptr == nullptr) {  // send eos
    VLOG(2) << "[EasyDK] [EncoderAx650] SendFrame(): Sent EOS frame encoder";
    if (MpsService::Instance().VEncSendFrame(venc_, nullptr, timeout_ms) < 0) {
      LOG(ERROR) << "[EasyDK] [EncoderAx650] SendFrame(): Sent EOS frame failed";
      return -1;
    }
    return 0;
  }

  if (surf->surface_list[0].width == create_params_.width &&
      surf->surface_list[0].height == create_params_.height) {  // no need scale
    AX_VIDEO_FRAME_INFO_T frame;
    BufSurfaceToVideoFrameInfo(surf, &frame);
    if (MpsService::Instance().VEncSendFrame(venc_, &frame, timeout_ms) < 0) {
      LOG(ERROR) << "[EasyDK] [EncoderAx650] SendFrame(): Sent frame failed";
      return -1;
    }
  } else {
    if (!output_surf_) {
      UniedkBufSurfaceCreateParams create_params;
      memset(&create_params, 0, sizeof(create_params));
      create_params.batch_size = 1;
      create_params.width = create_params_.width;
      create_params.height = create_params_.height;
      create_params.color_format = surf->surface_list[0].color_format;
      create_params.device_id = 0;
      create_params.mem_type = UNIEDK_BUF_MEM_VB_CACHED;

      if (UniedkBufPoolCreate(&surf_pool_, &create_params, 6) < 0) {
        LOG(ERROR) << "[EasyDK] [EncoderAx650] SendFrame(): Create pool failed";
        return -1;
      }

      if (UniedkBufSurfaceCreateFromPool(&output_surf_, surf_pool_) < 0) {
        LOG(ERROR) << "[EasyDK] [EncoderAx650] SendFrame(): Create BufSurface from pool failed";
        return -1;
      }
    }

    AX_VIDEO_FRAME_INFO_T frame;
    BufSurfaceToVideoFrameInfo(surf, &frame);

    AX_VIDEO_FRAME_INFO_T output;
    BufSurfaceToVideoFrameInfo(output_surf_, &output);

    MpsService::Instance().VguScaleCsc(&frame, &output);

    if (MpsService::Instance().VEncSendFrame(venc_, &output, timeout_ms) < 0) {
      LOG(ERROR) << "[EasyDK] [EncoderAx650] SendStream(): Sent frame failed";
      return -1;
    }
  }
  return 0;
}

// IVEncResult
void EncoderAx650::OnFrameBits(VEncFrameBits *frameBits) {
  UniedkVEncFrameBits stFrameBits;
  memset(&stFrameBits, 0, sizeof(UniedkVEncFrameBits));
  stFrameBits.bits = frameBits->bits;
  stFrameBits.len = frameBits->len;
  stFrameBits.pts = frameBits->pts;
  stFrameBits.pkt_type = frameBits->pkt_type;
  create_params_.OnFrameBits(&stFrameBits, create_params_.userdata);
}
void EncoderAx650::OnEos() { create_params_.OnEos(create_params_.userdata); }
void EncoderAx650::OnError(int errcode) {
  // TODO(gaoyujia): convert the error code
  create_params_.OnError(static_cast<int>(errcode), create_params_.userdata);
}

}  // namespace uniedk
