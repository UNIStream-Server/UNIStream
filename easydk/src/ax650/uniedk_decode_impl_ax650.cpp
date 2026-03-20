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

#include "uniedk_decode_impl_ax650.hpp"

#include <cstring>  // for memset
#include "glog/logging.h"
#include "axcl_rt.h"
#include "ax650_helper.hpp"
#include "axcl_native.h"

namespace uniedk {

// IDecoder
int DecoderAX650::Create(UniedkVdecCreateParams *params) {
  create_params_ = *params;
  AX_PAYLOAD_TYPE_E type;
  switch (create_params_.type) {
    case UNIEDK_VDEC_TYPE_H264:
      type = PT_H264;
      break;
    case UNIEDK_VDEC_TYPE_H265:
      type = PT_H265;
      break;
    case UNIEDK_VDEC_TYPE_JPEG:
      type = PT_JPEG;
      break;
    default:
      LOG(ERROR) << "[EasyDK] [DecoderAX650] Create(): Unsupported codec type: " << create_params_.type;
      return -1;
  }
  AX_IMG_FORMAT_E pix_fmt = AX_FORMAT_YUV420_SEMIPLANAR;
  if (create_params_.type == UNIEDK_VDEC_TYPE_JPEG) {
    switch (params->color_format) {
      case UNIEDK_BUF_COLOR_FORMAT_NV21:
        pix_fmt = AX_FORMAT_YUV420_SEMIPLANAR_VU;
        break;
      case UNIEDK_BUF_COLOR_FORMAT_NV12:
        pix_fmt = AX_FORMAT_YUV420_SEMIPLANAR;
        break;
      default:
        LOG(ERROR) << "[EasyDK] [DecoderAX650] Create(): Unsupported color format: " << params->color_format;
        return -1;
    }
  } else {
    switch (params->color_format) {
      case UNIEDK_BUF_COLOR_FORMAT_NV21:
        pix_fmt = AX_FORMAT_YUV420_SEMIPLANAR_VU;
        break;
      case UNIEDK_BUF_COLOR_FORMAT_NV12:
        pix_fmt = AX_FORMAT_YUV420_SEMIPLANAR;
        break;
      default:
        LOG(ERROR) << "[EasyDK] [DecoderAX650] Create(): Unsupported color format: " << params->color_format;
        return -1;
    }
  }

  vdec_ = MpsService::Instance().CreateVDec(this, type, params->max_width, params->max_height,
                                            params->frame_buf_num, pix_fmt);
  if (vdec_ == nullptr) {
    LOG(ERROR) << "[EasyDK] [DecoderAX650] Create(): Create decoder failed";
    return -1;
  }
  return 0;
}

int DecoderAX650::Destroy() {
  MpsService::Instance().DestroyVDec(vdec_);
  return 0;
}

int DecoderAX650::SendStream(const UniedkVdecStream *stream, int timeout_ms) {
  if (stream->bits == nullptr || stream->len == 0) {
    VLOG(2) << "[EasyDK] [DecoderAX650] SendStream(): Sent EOS packet to decoder";
    if (MpsService::Instance().VDecSendStream(vdec_, nullptr, 0) < 0) {
      LOG(ERROR) << "[EasyDK] [DecoderAX650] SendStream(): Sent EOS packet failed";
      return -1;
    }
    return 0;
  }

  AX_VDEC_STREAM_T input_data;
  memset(&input_data, 0, sizeof(input_data));
  input_data.u32StreamPackLen = stream->len;
  input_data.u64PTS = stream->pts;
  input_data.pu8Addr = stream->bits;
  input_data.bSkipDisplay = AX_FALSE;
  input_data.bEndOfFrame = AX_TRUE;
  input_data.bEndOfStream = AX_FALSE;
  if (MpsService::Instance().VDecSendStream(vdec_, &input_data, timeout_ms) < 0) {
    LOG(ERROR) << "[EasyDK] [DecoderAX650] SendStream(): Sent packet failed";
    return -1;
  }
  return 0;
}

// IVDecResult
void DecoderAX650::OnFrame(void *handle, const AX_VIDEO_FRAME_INFO_T *info) {
  // FIXME
  AX_VIDEO_FRAME_INFO_T *rectify_info = const_cast<AX_VIDEO_FRAME_INFO_T *>(info);
  rectify_info->stVFrame.u32Width += rectify_info->stVFrame.u32Width & 1;
  rectify_info->stVFrame.u32Height -= rectify_info->stVFrame.u32Height & 1;

  UniedkBufSurface *surf = nullptr;
  if (create_params_.GetBufSurf(&surf, rectify_info->stVFrame.u32Width, rectify_info->stVFrame.u32Height,
                               GetSurfFmt(info->stVFrame.enImgFormat), create_params_.surf_timeout_ms,
                               create_params_.userdata) < 0) {
    LOG(ERROR) << "[EasyDK] [DecoderAX650] OnFrame(): Get BufSurface failed";
    create_params_.OnError(-1, create_params_.userdata);
    MpsService::Instance().VDecReleaseFrame(handle, info);
    return;
  }
  AX_VIDEO_FRAME_INFO_T output;
  BufSurfaceToVideoFrameInfo(surf, &output);
  if (info->stVFrame.enImgFormat == output.stVFrame.enImgFormat) {
    UniedkBufSurfacePlaneParams &params = surf->surface_list[0].plane_params;
    for (size_t i = 0; i < params.num_planes; i++) {
	  AX_DMADIM_DESC_XD_T stDmaDesc;

	  stDmaDesc.u16Ntiles[0] = (i == 0) ? info->stVFrame.u32Height : info->stVFrame.u32Height / 2;

      stDmaDesc.tSrcInfo.u64PhyAddr = info->stVFrame.u64PhyAddr[i];
	  stDmaDesc.tSrcInfo.u32Imgw = info->stVFrame.u32Width;
	  stDmaDesc.tSrcInfo.u32Stride[0] = info->stVFrame.u32PicStride[0];
	  stDmaDesc.tSrcInfo.u32Stride[1] = info->stVFrame.u32PicStride[1];
	  stDmaDesc.tSrcInfo.u32Stride[2] = info->stVFrame.u32PicStride[2];

	  stDmaDesc.tDstInfo.u64PhyAddr = output.stVFrame.u64PhyAddr[i];
	  stDmaDesc.tDstInfo.u32Imgw = info->stVFrame.u32Width;
	  stDmaDesc.tDstInfo.u32Stride[0] = output.stVFrame.u32PicStride[0];
	  stDmaDesc.tDstInfo.u32Stride[1] = output.stVFrame.u32PicStride[1];
	  stDmaDesc.tDstInfo.u32Stride[2] = output.stVFrame.u32PicStride[2];

	  AXCL_DMA_MemCopyXD(stDmaDesc, AX_DMADIM_2D);
    }
  } else {
    //MpsService::Instance().VguScaleCsc(rectify_info, &output);
  }
  MpsService::Instance().VDecReleaseFrame(handle, info);

  surf->pts = rectify_info->stVFrame.u64PTS;
  create_params_.OnFrame(surf, create_params_.userdata);
  return;
}

void DecoderAX650::OnEos() { create_params_.OnEos(create_params_.userdata); }

void DecoderAX650::OnError(int errcode) {
  // TODO(gaoyujia): convert the error code
  create_params_.OnError(static_cast<int>(errcode), create_params_.userdata);
}

}  // namespace uniedk
