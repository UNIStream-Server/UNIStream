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
// for select
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <mutex>

#include "glog/logging.h"
#include "axcl_rt_memory.h"

#include "ax_service_impl_venc.hpp"

#define MAX_STREAM_SIZE (3*1024*1024)

namespace uniedk {

int sampleCommVencGetGopAttr(AX_VENC_GOP_MODE_E enGopMode, AX_VENC_GOP_ATTR_T *pstGopAttr) {
  switch (enGopMode) {
    case AX_VENC_GOPMODE_NORMALP:
      pstGopAttr->enGopMode = AX_VENC_GOPMODE_NORMALP;
      pstGopAttr->stNormalP.stPicConfig.s32QpOffset = 3;
      break;

    default:
	  LOG(INFO) << "[EasyDK] [MpsVenc] not support the gop mode !";
      return AX_ERR_VENC_ILLEGAL_PARAM;
      break;
  }
  return AX_SUCCESS;
}

static void MpsVencInit() {
	AX_VENC_MOD_ATTR_T attr = {};
	attr.enVencType = AX_VENC_MULTI_ENCODER;
	attr.stModThdAttr.bExplicitSched = AX_FALSE;
	attr.stModThdAttr.u32TotalThreadNum = 2;
	int ret = AXCL_VENC_Init(&attr);
	if (AX_SUCCESS != ret)
	{
		LOG(ERROR) << "[EasyDK] MpsVdecInit(): AXCL_VDEC_Init failed, ret = " << ret;
		return;
	}
}

int MpsVenc::Config(const MpsServiceConfig &config) {
  mps_config_ = config;

  MpsVencInit();
  std::unique_lock<std::mutex> lk(id_mutex_);
  for (int i = mps_config_.codec_id_start; i < kMaxMpsVecNum; i++) {
    id_q_.push(i);
  }
  return 0;
}

void *MpsVenc::Create(IVEncResult *result, VencCreateParam *params) {
  // check parameters, todo
  if (!result) {
    LOG(ERROR) << "[EasyDK] [MpsVenc] Create(): Result handler is null";
    return nullptr;
  }

  int id = GetId();
  if (id <= 0) {
    LOG(ERROR) << "[EasyDK] [MpsVenc] Create(): Get encoder id failed";
    return nullptr;
  }
  VEncCtx &ctx = venc_ctx[id - 1];
  if (ctx.created_) {
    LOG(ERROR) << "[EasyDK] [MpsVenc] Create(): Duplicated";
    return reinterpret_cast<void *>(id);
  }

  ctx.result_ = result;
  ctx.venc_chn_ = id - 1;
  memset(&ctx.chn_attr_, 0, sizeof(ctx.chn_attr_));
  ctx.chn_attr_.stVencAttr.enType = params->type;
  ctx.chn_attr_.stVencAttr.enLinkMode = AX_VENC_UNLINK_MODE;
  ctx.chn_attr_.stVencAttr.u32MaxPicWidth = AX_COMM_ALIGN(params->width, 8);
  ctx.chn_attr_.stVencAttr.u32MaxPicHeight = AX_COMM_ALIGN(params->height, 8);
  ctx.chn_attr_.stVencAttr.u32PicWidthSrc = params->width;
  ctx.chn_attr_.stVencAttr.u32PicHeightSrc = params->height;
  ctx.chn_attr_.stVencAttr.u32BufSize =
      ctx.chn_attr_.stVencAttr.u32MaxPicWidth * ctx.chn_attr_.stVencAttr.u32MaxPicHeight * 3 / 2;
  ctx.chn_attr_.stVencAttr.u8InFifoDepth = 2;
  ctx.chn_attr_.stVencAttr.u8OutFifoDepth = 2;

  AX_VENC_GOP_ATTR_T &stGopAttr = ctx.chn_attr_.stGopAttr;
  sampleCommVencGetGopAttr(AX_VENC_GOPMODE_NORMALP, &stGopAttr);

  uint32_t u32FrameRate = static_cast<uint32_t>(params->frame_rate);
  u32FrameRate = u32FrameRate > 1 ? u32FrameRate : 30;
  u32FrameRate = u32FrameRate < 120 ? u32FrameRate : 120;

  AX_FRAME_RATE_CTRL_T &stFrameRate = ctx.chn_attr_.stRcAttr.stFrameRate;
  stFrameRate.fSrcFrameRate = u32FrameRate;
  stFrameRate.fDstFrameRate = u32FrameRate;

  if (params->type == PT_H264) {
    AX_VENC_H264_VBR_T &stH264Vbr = ctx.chn_attr_.stRcAttr.stH264Vbr;
    stH264Vbr.u32Gop = 30;
    stH264Vbr.u32StatTime = 1;  // u32StatTime;
    stH264Vbr.u32MaxQp = 28;
    stH264Vbr.u32MinQp = 18;
    stH264Vbr.u32MaxIQp = 25;
    stH264Vbr.u32MinIQp = 15;
	stH264Vbr.s32IntraQpDelta = -2;
	stH264Vbr.u32ChangePos = 90;

    if (params->bitrate > 0) {
      stH264Vbr.u32MaxBitRate = params->bitrate;
    } else {
      stH264Vbr.u32MaxBitRate = 1024 * 2 + 2048 * u32FrameRate / 30;
    }

    ctx.chn_attr_.stRcAttr.enRcMode = AX_VENC_RC_MODE_H264VBR;
    ctx.chn_attr_.stVencAttr.enProfile = AX_VENC_H264_MAIN_PROFILE;
	ctx.chn_attr_.stVencAttr.enLevel = AX_VENC_H264_LEVEL_1;
	ctx.chn_attr_.stVencAttr.enTier = AX_VENC_HEVC_MAIN_TIER;
  } else if (params->type == PT_H265) {
    AX_VENC_H265_VBR_T &stH265Vbr = ctx.chn_attr_.stRcAttr.stH265Vbr;
    stH265Vbr.u32Gop = 30;
    stH265Vbr.u32StatTime = 1;  // u32StatTime;
    stH265Vbr.u32MaxQp = 28;
    stH265Vbr.u32MinQp = 18;
    stH265Vbr.u32MaxIQp = 25;
    stH265Vbr.u32MinIQp = 15;
	stH265Vbr.s32IntraQpDelta = -2;
	stH265Vbr.u32ChangePos = 90;

    if (params->bitrate > 0) {
      stH265Vbr.u32MaxBitRate = params->bitrate;
    } else {
      stH265Vbr.u32MaxBitRate = 1024 * 2 + 2048 * u32FrameRate / 30;
    }

    ctx.chn_attr_.stRcAttr.enRcMode = AX_VENC_RC_MODE_H265VBR;
    ctx.chn_attr_.stVencAttr.enProfile = AX_VENC_HEVC_MAIN_PROFILE;
	ctx.chn_attr_.stVencAttr.enLevel = AX_VENC_HEVC_LEVEL_1;
    ctx.chn_attr_.stVencAttr.enTier = AX_VENC_HEVC_MAIN_TIER;
  } else if (params->type == PT_JPEG) {
    ctx.chn_attr_.stVencAttr.u32BufSize =
      AX_COMM_ALIGN(AX_COMM_ALIGN(params->width, 16) * AX_COMM_ALIGN(params->height, 16), 4096);
  } else {
    ReturnId(id);
    LOG(ERROR) << "[EasyDK] [MpsVenc] Create(): Unsupported codec type: " << params->type;
    return nullptr;
  }

  int ret = AXCL_VENC_CreateChn(ctx.venc_chn_, &ctx.chn_attr_);
  if (AX_SUCCESS != ret) {
    ReturnId(id);
    LOG(ERROR) << "[EasyDK] [MpsVenc] Create(): AXCL_VENC_CreateChn failed, ret = " << ret;
    return nullptr;
  }

  if (params->type == PT_JPEG) {
    AX_VENC_JPEG_PARAM_T pstJpegParam;
    ret = AXCL_VENC_GetJpegParam(ctx.venc_chn_, &pstJpegParam);
    if (AX_SUCCESS != ret) {
      ReturnId(id);
      LOG(ERROR) << "[EasyDK] [MpsVenc] Create(): AXCL_VENC_GetJpegParam failed, ret = " << ret;
      return nullptr;
    }

    pstJpegParam.u32Qfactor = 90;
    AXCL_VENC_SetJpegParam(ctx.venc_chn_, &pstJpegParam);
    if (AX_SUCCESS != ret) {
      ReturnId(id);
      LOG(ERROR) << "[EasyDK] [MpsVenc] Create(): AXCL_VENC_SetJpegParam failed, ret = " << ret;
      return nullptr;
    }
  }

  memset(&ctx.chn_param_, 0, sizeof(ctx.chn_param_));
  ctx.chn_param_.s32RecvPicNum = -1;
  ret = AXCL_VENC_StartRecvFrame(ctx.venc_chn_, &ctx.chn_param_);
  if (AX_SUCCESS != ret) {
    LOG(ERROR) << "[EasyDK] [MpsVenc] Create(): AXCL_VENC_StartRecvFrame failed, ret = " << ret;
    goto err_exit;
  }

  ctx.grp_Id_ = ctx.venc_chn_;
  AXCL_VENC_SelectClearGrp(ctx.grp_Id_);
  ret = AXCL_VENC_SelectGrpAddChn(ctx.grp_Id_, ctx.venc_chn_);
  if (AX_SUCCESS != ret) {
    LOG(ERROR) << "[EasyDK] [MpsVenc] Create(): AXCL_VENC_SelectGrpAddChn failed, ret = " << ret;
    goto err_exit;
  }

  ctx.host_mem_ = malloc(MAX_STREAM_SIZE);
  if (NULL == ctx.host_mem_) {
    LOG(ERROR) << "[EasyDK] [MpsVenc] Create(): malloc failed, len = " << MAX_STREAM_SIZE;
    goto err_exit;
  }

  ctx.eos_sent_ = false;
  ctx.error_flag_ = false;
  ctx.created_ = true;
  VLOG(2) << "[EasyDK] [MpsVenc] Create(): Done";
  return reinterpret_cast<void *>(id);

err_exit:
  AXCL_VENC_StopRecvFrame(ctx.venc_chn_);
  AXCL_VENC_DestroyChn(ctx.venc_chn_);
  ReturnId(id);
  return nullptr;
}

int MpsVenc::Destroy(void *handle) {
  int64_t id = reinterpret_cast<int64_t>(handle);
  if (id <= 0 || id > kMaxMpsVecNum) {
    LOG(ERROR) << "[EasyDK] [MpsVenc] Destroy(): Handle is invalid";
    return -1;
  }

  VEncCtx &ctx = venc_ctx[id - 1];
  if (!ctx.created_) {
    LOG(INFO) << "[EasyDK] [MpsVenc] Destroy(): Handle is not created";
    return 0;
  }

  // if error happened, destroy directly, eos maybe not be transmitted from the encoder
  if (!ctx.error_flag_ && !ctx.eos_sent_) {
    SendFrame(handle, nullptr, 0);
  }

  int codec_ret = AXCL_VENC_StopRecvFrame(ctx.venc_chn_);
  if (AX_SUCCESS != codec_ret) {
    LOG(ERROR) << "[EasyDK] [MpsVenc] Destroy(): AXCL_VENC_StopRecvFrame failed, ret = " << codec_ret;
  }

  codec_ret = AXCL_VENC_DestroyChn(ctx.venc_chn_);
  if (AX_SUCCESS != codec_ret) {
    LOG(ERROR) << "[EasyDK] [MpsVenc] Destroy(): AXCL_VENC_DestroyChn failed, ret = " << codec_ret;
  }

  free(ctx.host_mem_);
  ctx.host_mem_ = nullptr;
  
  ctx.venc_chn_ = -1;
  ctx.eos_sent_ = false;
  ctx.error_flag_ = false;
  ctx.created_ = false;
  ReturnId(id);
  return 0;
}

int MpsVenc::CheckHandleEos(void *handle) {
  int64_t id = reinterpret_cast<int64_t>(handle);
  VEncCtx &ctx = venc_ctx[id - 1];
  AX_VENC_CHN_STATUS_T stStatus;
  memset(&stStatus, 0, sizeof(stStatus));
  if (AXCL_VENC_QueryStatus(ctx.venc_chn_, &stStatus) != AX_SUCCESS) {
    ctx.error_flag_ = true;
    if (ctx.result_) {
      ctx.result_->OnError(AX_ERR_VENC_NOT_PERMIT);
    }
    LOG(ERROR) << "[EasyDK] [MpsVenc] CheckHandleEos(): AXCL_VENC_QueryStatus failed";
    return -1;
  }
  if (0 == stStatus.u32LeftPics && stStatus.u32LeftStreamFrames == 0 && stStatus.u32LeftStreamBytes == 0) {
    if (ctx.result_) {
      ctx.result_->OnEos();
    }
    return 0;
  }
  return 1;
}

void MpsVenc::OnFrameBits(void *handle, AX_VENC_STREAM_T *pst_stream) {
  int64_t id = reinterpret_cast<int64_t>(handle);
  VEncCtx &ctx = venc_ctx[id - 1];
  if (!ctx.result_) {
    LOG(ERROR) << "[EasyDK] [MpsVenc] OnFrameBits(): Result handler is null";
    return;
  }
  
  VEncFrameBits framebits;
  AX_VENC_PACK_T *pkt = &pst_stream->stPack;
  framebits.bits = pkt->pu8Addr;
  framebits.len = pkt->u32Len;
  framebits.pkt_type = UNIEDK_VENC_PACKAGE_TYPE_FRAME;
  if (PT_H264 == ctx.chn_attr_.stVencAttr.enType)
  {
    for (AX_U32 i = 0; i < pkt->u32NaluNum; i++)
    {
      if (AX_H264E_NALU_SPS == pkt->stNaluInfo[i].unNaluType.enH264EType ||
          AX_H264E_NALU_PPS == pkt->stNaluInfo[i].unNaluType.enH264EType ||
          AX_H264E_NALU_IDRSLICE == pkt->stNaluInfo[i].unNaluType.enH264EType) {
        framebits.pkt_type = UNIEDK_VENC_PACKAGE_TYPE_KEY_FRAME;
		break;
      }
    }
  }
  else if (PT_H265 == ctx.chn_attr_.stVencAttr.enType)
  {
    for (AX_U32 i = 0; i < pkt->u32NaluNum; i++)
    {
      if (AX_H265E_NALU_SPS == pkt->stNaluInfo[i].unNaluType.enH265EType ||
          AX_H265E_NALU_PPS == pkt->stNaluInfo[i].unNaluType.enH265EType ||
          AX_H265E_NALU_VPS == pkt->stNaluInfo[i].unNaluType.enH265EType ||
          AX_H265E_NALU_IDRSLICE == pkt->stNaluInfo[i].unNaluType.enH265EType) {
        framebits.pkt_type = UNIEDK_VENC_PACKAGE_TYPE_KEY_FRAME;
		break;
      }
    }
  }
  
  framebits.pts = pkt->u64PTS;
  ctx.result_->OnFrameBits(&framebits);
}

int MpsVenc::SendFrame(void *handle, const AX_VIDEO_FRAME_INFO_T *pst_frame, AX_S32 milli_sec) {
  int64_t id = reinterpret_cast<int64_t>(handle);
  if (id <= 0 || id > kMaxMpsVecNum) {
    LOG(ERROR) << "[EasyDK] [MpsVenc] SendFrame(): Handle is invalid";
    return -1;
  }
  VLOG(5) << "[EasyDK] [MpsVenc] SendFrame(): Frame width: "<< pst_frame->stVFrame.u32Width << ", height: "
          << pst_frame->stVFrame.u32Height << ", stride:" << pst_frame->stVFrame.u32PicStride[0];

  VEncCtx &ctx = venc_ctx[id - 1];
  if (!ctx.created_) {
    LOG(INFO) << "[EasyDK] [MpsVenc] SendFrame(): Handle is not created";
    return -1;
  }

  bool send_done = false;
  if (nullptr == pst_frame) {
    VLOG(2) << "[EasyDK] [MpsVenc] SendFrame(): Send EOS";
    if (ctx.eos_sent_) send_done = true;
    ctx.eos_sent_ = true;
  } else {
    if (ctx.eos_sent_) {
      LOG(ERROR) << "[EasyDK] [MpsVenc] SendFrame(): EOS has been sent, process frameInfo failed, pts:"
                 << pst_frame->stVFrame.u64PTS;
      return -1;
    }
    if (ctx.error_flag_) {
      LOG(ERROR) << "[EasyDK] [MpsVenc] SendFrame(): Error occurred in encoder, process frameInfo failed, pts:"
                 << pst_frame->stVFrame.u64PTS;
      return -1;
    }
  }

  AX_VIDEO_FRAME_INFO_T stFrameInfo4Venc = {0};
  int count = 4;  // FIXME
  while (!ctx.error_flag_) {
    // try send frame_info
    if (!send_done) {
      if (!pst_frame) {
        send_done = true;
        continue;
      }

	  stFrameInfo4Venc = *pst_frame;
	  stFrameInfo4Venc.stVFrame.u64VirAddr[0] = 0;
	  stFrameInfo4Venc.stVFrame.u64VirAddr[1] = 0;
	
      int ret = AXCL_VENC_SendFrame(ctx.venc_chn_, &stFrameInfo4Venc, 1000);
      if (ret == AX_SUCCESS) {
        VLOG(5) << "[EasyDK] [MpsVenc] SendFrame(): AXCL_VENC_SendFrame pts = " << pst_frame->stVFrame.u64PTS;
        send_done = true;
        continue;
      }
	  
      if (--count < 0) {
        ctx.error_flag_ = true;
        LOG(ERROR) << "[EasyDK] [MpsVenc] SendFrame(): Send frame timeout";
        if (ctx.result_) {
          ctx.result_->OnError(AX_ERR_VENC_NOT_PERMIT);
        }
        return -1;
      }
    }

    // try get bitstream:
    //  1. polling
    //  2. get bitstream and handle it
    while (1) {
	  AX_CHN_STREAM_STATUS_T stChnStrmState = {};
	  int ret = AXCL_VENC_SelectGrp(ctx.grp_Id_, &stChnStrmState, 5);
      if (AX_SUCCESS != ret) {
         //LOGE(SOURCE) << "[" << stream_id_ << "]: " << "select timeout\n";
        if (ctx.eos_sent_ && send_done) {
          int value = CheckHandleEos(handle);
          if (value < 0) {
            return -1;
          } else if (value == 0) {
            return 0;
          }
          // going on ..
          continue;
        }
        break;
      }

	  if (0 == stChnStrmState.u32TotalChnNum || 
	  	ctx.venc_chn_ != (VENC_CHN) stChnStrmState.au32ChnIndex[0]){
        LOG(ERROR) << "[EasyDK] [MpsVenc] SendFrame(): select no data";
        return -1;
      }
	  
      AX_VENC_STREAM_T stStream;
      memset(&stStream, 0, sizeof(stStream));
      ret = AXCL_VENC_GetStream(ctx.venc_chn_, &stStream, 10);
      if (ret == AX_SUCCESS) {
		axclrtMemcpy(ctx.host_mem_, (void *)stStream.stPack.ulPhyAddr, stStream.stPack.u32Len, AXCL_MEMCPY_DEVICE_TO_HOST);

		AX_VENC_STREAM_T stStream4Host = stStream;
		stStream4Host.stPack.pu8Addr = (AX_U8 *)ctx.host_mem_;
        OnFrameBits(handle, &stStream4Host);
		
        AXCL_VENC_ReleaseStream(ctx.venc_chn_, &stStream);
        if (ctx.eos_sent_ && send_done) {
          int value = CheckHandleEos(handle);
          if (value < 0) {
            LOG(ERROR) << "[EasyDK] [MpsVenc] SendFrame(): CheckHandleEos failed";
            return -1;
          } else if (value == 0) {
            return 0;
          }
          // going on ...
        }
      } else {
        ctx.error_flag_ = true;
        LOG(ERROR) << "[EasyDK] [MpsVenc] SendFrame(): AXCL_VENC_GetStream failed, ret = " << ret;
        if (ctx.result_) {
          ctx.result_->OnError(AX_ERR_VENC_NOT_PERMIT);
        }
        return -1;
      }
    }

    if (!ctx.eos_sent_ && send_done) {
      break;
    }
  }
  return 0;
}

}  // namespace uniedk
