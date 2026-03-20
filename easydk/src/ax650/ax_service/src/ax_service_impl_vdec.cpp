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

#include "ax_buffer_tool.h"
#include "ax_service_impl_vdec.hpp"

namespace uniedk {

/*
 *  plat has global mode params...hardcode at this moment!!!
 *  FIXME
 */
static void MpsVdecInit() {
	AX_VDEC_MOD_ATTR_T attr = {0};
	attr.u32MaxGroupCount = kMaxMpsVdecNum;
	attr.enDecModule = AX_ENABLE_BOTH_VDEC_JDEC;
	int ret = AXCL_VDEC_Init(&attr);
	if (AX_SUCCESS != ret)
	{
		LOG(ERROR) << "[EasyDK] MpsVdecInit(): AXCL_VDEC_Init failed, ret = " << ret;
		return;
	}
}

int MpsVdec::Config(const MpsServiceConfig &config) {
  mps_config_ = config;

  MpsVdecInit();
  std::unique_lock<std::mutex> lk(id_mutex_);
  for (int i = mps_config_.codec_id_start; i < kMaxMpsVdecNum; i++) {
    id_q_.push(i);
  }
  return 0;
}

void *MpsVdec::Create(IVDecResult *result, AX_PAYLOAD_TYPE_E type, int max_width, int max_height, int buf_num,
                      AX_IMG_FORMAT_E pix_fmt) {
  // check parameters, todo
  if (!result) {
    LOG(ERROR) << "[EasyDK] [MpsVdec] Create(): Result handler is null";
    return nullptr;
  }
  switch (type) {
    case PT_H264:
    case PT_H265:
    case PT_JPEG:
      break;
    default: {
      LOG(ERROR) << "[EasyDK] [MpsVdec] Create(): Unsupported codec type: " << type;
      return nullptr;
    }
  }
  //
  int id = GetId();
  if (id <= 0) {
    LOG(ERROR) << "[EasyDK] [MpsVdec] Create(): Get decoder id failed";
    return nullptr;
  }
  VDecCtx &ctx = vdec_ctx_[id - 1];
  if (ctx.created_) {
    LOG(ERROR) << "[EasyDK] [MpsVdec] Create(): Duplicated";
    return reinterpret_cast<void *>(id);
  }
  ctx.result_ = result;
  ctx.vdec_chn_ = id - 1;

  memset(&ctx.grp_attr_, 0, sizeof(ctx.grp_attr_));
  ctx.grp_attr_.enCodecType = type;
  ctx.grp_attr_.enInputMode = AX_VDEC_INPUT_MODE_FRAME;
  ctx.grp_attr_.u32MaxPicWidth = AX_COMM_ALIGN(max_width, 16);
  ctx.grp_attr_.u32MaxPicHeight = AX_COMM_ALIGN(max_height, 16);
  ctx.grp_attr_.u32StreamBufSize = ctx.grp_attr_.u32MaxPicWidth * ctx.grp_attr_.u32MaxPicHeight * 3 / 2;
  ctx.grp_attr_.bSdkAutoFramePool = AX_TRUE;

  VLOG(2) << "Type: " << ctx.grp_attr_.enCodecType
          << " WxH: " << ctx.grp_attr_.u32MaxPicWidth << "x" << ctx.grp_attr_.u32MaxPicHeight
          << " Stream size: " << ctx.grp_attr_.u32StreamBufSize;
  
  int ret = AXCL_VDEC_CreateGrp(ctx.vdec_chn_, &ctx.grp_attr_);
  if (AX_SUCCESS != ret) {
    ReturnId(id);
    LOG(ERROR) << "[EasyDK] [MpsVdec] Create(): AXCL_VDEC_CreateGrp failed, ret = " << ret;
    return nullptr;
  }

  memset(&ctx.grp_param_, 0, sizeof(ctx.grp_param_));
  ctx.grp_param_.stVdecVideoParam.enVdecMode = VIDEO_DEC_MODE_IPB;
  ctx.grp_param_.stVdecVideoParam.enOutputOrder = AX_VDEC_OUTPUT_ORDER_DEC;
  ctx.grp_param_.f32SrcFrmRate = 150.0;
  ret = AXCL_VDEC_SetGrpParam(ctx.vdec_chn_, &ctx.grp_param_);
  if (AX_SUCCESS != ret) {
    LOG(ERROR) << "[EasyDK] [MpsVdec] Create(): AXCL_VDEC_SetGrpParam failed, ret = " << ret;
    goto err_exit;
  }

  memset(&ctx.chn_attr_, 0, sizeof(ctx.chn_attr_));
  ctx.chn_attr_.u32PicWidth  = max_width;
  ctx.chn_attr_.u32PicHeight = max_height;
  if (PT_JPEG == ctx.grp_attr_.enCodecType)
  {
    ctx.chn_attr_.u32FrameStride = AX_COMM_ALIGN(ctx.chn_attr_.u32PicWidth, 1 << (6));
  }
  else
  {
    ctx.chn_attr_.u32FrameStride = AX_COMM_ALIGN(ctx.chn_attr_.u32PicWidth, 1 << (8));
  }
  ctx.chn_attr_.u32OutputFifoDepth = 1;
  ctx.chn_attr_.u32FrameBufCnt = 2;
  ctx.chn_attr_.u32FrameBufSize = AX_VDEC_GetPicBufferSize(ctx.chn_attr_.u32PicWidth, ctx.chn_attr_.u32PicHeight, 
                                                           pix_fmt, &ctx.chn_attr_.stCompressInfo, 
                                                           ctx.grp_attr_.enCodecType);
  ctx.chn_attr_.enOutputMode = AX_VDEC_OUTPUT_ORIGINAL;
  ctx.chn_attr_.enImgFormat = pix_fmt;
  ret = AXCL_VDEC_SetChnAttr(ctx.vdec_chn_, 0, &ctx.chn_attr_);
  if (AX_SUCCESS != ret) {
    LOG(ERROR) << "[EasyDK] [MpsVdec] Create(): AXCL_VDEC_SetChnAttr failed, ret = " << ret;
    goto err_exit;
  }

  ret = AXCL_VDEC_EnableChn(ctx.vdec_chn_, 0);
  if (AX_SUCCESS != ret) {
    LOG(ERROR) << "[EasyDK] [MpsVdec] Create(): AXCL_VDEC_EnableChn failed, ret = " << ret;
    goto err_exit;
  }

  AX_VDEC_RECV_PIC_PARAM_T stRecvParam;
  stRecvParam.s32RecvPicNum = -1;
  
  ret = AXCL_VDEC_StartRecvStream(ctx.vdec_chn_, &stRecvParam);
  if (AX_SUCCESS != ret) {
    LOG(ERROR) << "[EasyDK] [MpsVdec] Create(): AXCL_VDEC_StartRecvStream failed, ret = " << ret;
    goto err_exit;
  }

  ctx.eos_sent_ = false;
  ctx.error_flag_ = false;
  ctx.created_ = true;
  VLOG(2) << "[EasyDK] [MpsVdec] Create(): Done";
  return reinterpret_cast<void *>(id);

err_exit:
  AXCL_VDEC_StopRecvStream(ctx.vdec_chn_);
  AXCL_VDEC_DestroyGrp(ctx.vdec_chn_);
  ReturnId(id);
  return nullptr;
}

int MpsVdec::Destroy(void *handle) {
  int64_t id = reinterpret_cast<int64_t>(handle);
  if (id <= 0 || id > kMaxMpsVdecNum) {
    LOG(ERROR) << "[EasyDK] [MpsVdec] Destroy(): Handle is invalid";
    return -1;
  }

  VDecCtx &ctx = vdec_ctx_[id - 1];
  if (!ctx.created_) {
    LOG(INFO) << "[EasyDK] [MpsVdec] Destroy(): Handle is not created";
    return 0;
  }

  // if error happened, destroy directly, eos maybe not be transmitted from the decoder
  if (!ctx.error_flag_ && !ctx.eos_sent_) {
    //SendStream(handle, nullptr, 0);
  }

  int codec_ret = AXCL_VDEC_StopRecvStream(ctx.vdec_chn_);
  if (AX_SUCCESS != codec_ret) {
    LOG(ERROR) << "[EasyDK] [MpsVdec] Destroy(): AXCL_VDEC_StopRecvStream failed, ret = " << codec_ret;
  }

  codec_ret = AXCL_VDEC_DestroyGrp(ctx.vdec_chn_);
  if (AX_SUCCESS != codec_ret) {
    LOG(ERROR) << "[EasyDK] [MpsVdec] Destroy(): AXCL_VDEC_DestroyGrp failed, ret = " << codec_ret;
  }
  ctx.vdec_chn_ = -1;
  ctx.eos_sent_ = false;
  ctx.error_flag_ = false;
  ctx.created_ = false;
  ReturnId(id);
  return 0;
}

int MpsVdec::CheckHandleEos(void *handle) {
  int64_t id = reinterpret_cast<int64_t>(handle);
  VDecCtx &ctx = vdec_ctx_[id - 1];
  AX_VDEC_GRP_STATUS_T stStatus;
  memset(&stStatus, 0, sizeof(stStatus));
  if (AXCL_VDEC_QueryStatus(ctx.vdec_chn_, &stStatus) != AX_SUCCESS) {
    ctx.error_flag_ = true;
    if (ctx.result_) {
      ctx.result_->OnError(AX_ERR_VDEC_UNKNOWN);
    }
    LOG(ERROR) << "[EasyDK] [MpsVdec] CheckHandleEos(): AXCL_VDEC_QueryStatus failed";
    return -1;
  }
  if (0 == stStatus.u32LeftPics[0] && stStatus.u32LeftStreamFrames == 0) {
    if (ctx.result_) {
      ctx.result_->OnEos();
    }
    return 0;
  }
  return 1;
}

int MpsVdec::ReleaseFrame(void *handle, const AX_VIDEO_FRAME_INFO_T *info) {
  int64_t id = reinterpret_cast<int64_t>(handle);
  if (id <= 0 || id > kMaxMpsVdecNum) {
    LOG(ERROR) << "[EasyDK] [MpsVdec] ReleaseFrame(): Handle is invalid";
    return -1;
  }
  VDecCtx &ctx = vdec_ctx_[id - 1];
  AXCL_VDEC_ReleaseChnFrame(ctx.vdec_chn_, 0, info);
  return 0;
}

int MpsVdec::SendStream(void *handle, const AX_VDEC_STREAM_T *pst_stream, AX_S32 milli_sec) {
  int64_t id = reinterpret_cast<int64_t>(handle);
  if (id <= 0 || id > kMaxMpsVdecNum) {
    LOG(ERROR) << "[EasyDK] [MpsVdec] SendStream(): Handle is invalid";
    return -1;
  }

  VDecCtx &ctx = vdec_ctx_[id - 1];
  AX_VDEC_STREAM_T input_data;
  bool send_done = false;
  memset(&input_data, 0, sizeof(input_data));
  if (nullptr == pst_stream) {
    VLOG(2) << "[EasyDK] [MpsVdec] SendStream(): Send EOS";
    if (ctx.eos_sent_) send_done = true;
    ctx.eos_sent_ = true;
    input_data.u32StreamPackLen = 0;
    input_data.u64PTS = 0;
    input_data.bSkipDisplay = AX_FALSE;
    input_data.bEndOfFrame = AX_TRUE;
    input_data.bEndOfStream = AX_TRUE;
  } else {
    if (ctx.eos_sent_) {
      LOG(ERROR) << "[EasyDK] [MpsVdec] SendStream(): EOS has been sent, process packet failed, pts:"
                 << pst_stream->u64PTS;
      return -1;
    }
    if (ctx.error_flag_) {
      LOG(ERROR) << "[EasyDK] [MpsVdec] SendStream(): Error occurred in decoder, process packet failed, pts:"
                 << pst_stream->u64PTS;
      return -1;
    }
    input_data = *pst_stream;
    input_data.bEndOfFrame = AX_TRUE;
    input_data.bEndOfStream = AX_FALSE;
  }

  int count = 4;  // FIXME
  while (!ctx.error_flag_) {
    // try SendStream
    if (!send_done) {
      // JPEG dec didn't handle null packet
      if (ctx.eos_sent_ && (PT_JPEG == ctx.grp_attr_.enCodecType)) {
        send_done = true;
        continue;
      }
      int ret = AXCL_VDEC_SendStream(ctx.vdec_chn_, &input_data, 1000);
      if (ret == AX_SUCCESS) {
        send_done = true;
        continue;
      } else if (ret != AX_ERR_VDEC_BUF_FULL) {
        LOG(ERROR) << "[EasyDK] [MpsVdec] SendStream(): Send stream failed, ret = " << ret;
        ctx.error_flag_ = true;
        if (ctx.result_) {
          ctx.result_->OnError(ret);
        }
        return -1;
      }
	  // get picture and process it
      AX_VIDEO_FRAME_INFO_T frame_info;
      memset(&frame_info, 0, sizeof(frame_info));
      ret = AXCL_VDEC_GetChnFrame(ctx.vdec_chn_, 0, &frame_info, 10);
      if (ret == AX_SUCCESS) {
        if (ctx.result_) {
          ctx.result_->OnFrame(handle, &frame_info);
        } else {
          AXCL_VDEC_ReleaseChnFrame(ctx.vdec_chn_, 0, &frame_info);
        }
      } else if (AX_ERR_VDEC_BUF_EMPTY != ret) {
        ctx.error_flag_ = true;
        LOG(ERROR) << "[EasyDK] [MpsVdec] SendFrame(): AXCL_VDEC_GetChnFrame failed, ret = " << ret;
        if (ctx.result_) {
          ctx.result_->OnError(ret);
        }
        return -1;
      }

	  if (--count < 0) {
        ctx.error_flag_ = true;
        if (ctx.result_) {
          ctx.result_->OnError(AX_ERR_VDEC_UNKNOWN);
        }
        LOG(ERROR) << "[EasyDK] [MpsVdec] SendStream(): Send stream timeout";
        return -1;
      }
    }

    // try getFrame:
    //  1. polling
    //  2. get picture and handle it
    while (1) {

	  if (ctx.eos_sent_ && send_done) {
        int value = CheckHandleEos(handle);
        if (value < 0) {
          LOG(ERROR) << "[EasyDK] [MpsVdec] SendStream(): CheckHandleEos failed, ret = " << value;
          return -1;
        } else if (value == 0) {
          return 0;
        }
        // going on ...
      }
      // get picture and process it
      AX_VIDEO_FRAME_INFO_T frame_info;
      memset(&frame_info, 0, sizeof(frame_info));
      int ret = AXCL_VDEC_GetChnFrame(ctx.vdec_chn_, 0, &frame_info, 10);
      if (ret == AX_SUCCESS) {
        if (ctx.result_) {
          ctx.result_->OnFrame(handle, &frame_info);
        } else {
          AXCL_VDEC_ReleaseChnFrame(ctx.vdec_chn_, 0, &frame_info);
        }
      } else if (AX_ERR_VDEC_FLOW_END == ret) {
        return 0;
      } else if (AX_ERR_VDEC_BUF_EMPTY != ret) {
        ctx.error_flag_ = true;
        LOG(ERROR) << "[EasyDK] [MpsVdec] SendStream(): AXCL_VDEC_GetChnFrame failed, ret = " << ret;
        if (ctx.result_) {
          ctx.result_->OnError(ret);
        }
        return -1;
      } else {
        return 0;
      }
    }

    if (!ctx.eos_sent_ && send_done) {
      break;
    }
  }
  return 0;
}

}  // namespace uniedk
