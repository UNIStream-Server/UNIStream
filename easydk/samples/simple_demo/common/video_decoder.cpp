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

#include <glog/logging.h>
// #include <libyuv.h>

#include <algorithm>
#include <memory>
#include <string>

#include "video_decoder.h"
#include "stream_runner.h"

#include "util/utils.hpp"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

// ------------------------------- DecodeImpl --------------------------------------
class EasyDecodeImpl : public VideoDecoderImpl {
 public:
  EasyDecodeImpl(VideoDecoder* interface, IDecodeEventHandle* handle, int device_id)
      : VideoDecoderImpl(interface, handle, device_id) {}
  bool Init() override {
    VideoInfo& info = interface_->GetVideoInfo();
    bsf_ = nullptr;

    UniedkVdecCreateParams create_params = {};
    create_params.device_id  = device_id_;

    create_params.max_width = info.width;
    create_params.max_height = info.height;
    create_params.frame_buf_num = 12;  // for CE3226
    create_params.surf_timeout_ms = 5000;
    create_params.userdata = this;
    create_params.GetBufSurf = GetBufSurface_;
    create_params.OnFrame = OnFrame_;
    create_params.OnEos = OnDecodeEos_;
    create_params.OnError = OnError_;

    if (AV_CODEC_ID_H264 == info.codec_id) {
      create_params.type = UNIEDK_VDEC_TYPE_H264;
      bsf_ = av_bsf_get_by_name("h264_mp4toannexb");
    } else if (AV_CODEC_ID_HEVC == info.codec_id) {
      create_params.type = UNIEDK_VDEC_TYPE_H265;
      bsf_ = av_bsf_get_by_name("hevc_mp4toannexb");
    } else if (AV_CODEC_ID_MJPEG == info.codec_id) {
      create_params.type = UNIEDK_VDEC_TYPE_JPEG;
    } else {
      LOG(ERROR) << "[EasyDK Samples] [EasyDecodeImpl] Init(): Unsupported codec id: " << info.codec_id;
      return false;
    }

	av_bsf_alloc(bsf_, &bsf_ctx_);

    if (IsEdgePlatform(device_id_)) {
      create_params.color_format = UNIEDK_BUF_COLOR_FORMAT_NV21;
    } else {
      create_params.color_format = UNIEDK_BUF_COLOR_FORMAT_NV12;
    }

    if (CreateSurfacePool(&surf_pool_, create_params.max_width, create_params.max_height) < 0) {
      LOG(ERROR) << "[EasyDK Samples] [EasyDecodeImpl] Init(): Create BufSurface pool failed ";
      return false;
    }

    VLOG(1) << "[EasyDK Samples] [EasyDecodeImpl] Init(): surf_pool:" << surf_pool_;

    if (UniedkVdecCreate(&vdec_, &create_params) < 0) {
      LOG(ERROR) << "[EasyDK Samples] [EasyDecodeImpl] Init(): Create decoder failed";
      return false;
    }

    return true;
  }

  int CreateSurfacePool(void** surf_pool, int width, int height) {
    UniedkBufSurfaceCreateParams create_params;
    memset(&create_params, 0, sizeof(create_params));
    create_params.batch_size = 1;
    create_params.width = width;
    create_params.height = height;
    create_params.color_format = UNIEDK_BUF_COLOR_FORMAT_NV12;
    create_params.device_id = device_id_;

    if (IsEdgePlatform(device_id_)) {
      create_params.mem_type = UNIEDK_BUF_MEM_VB_CACHED;
    } else {
      create_params.mem_type = UNIEDK_BUF_MEM_DEVICE;
    }

    if (UniedkBufPoolCreate(surf_pool, &create_params, 16) < 0) {
      LOG(ERROR) << "[EasyDK Samples] [EasyDecodeImpl] CreateSurfacePool(): Create pool failed";
      return -1;
    }

    return 0;
  }

  bool FeedPacket(const AVPacket* packet) override {
    AVPacket* parsed_pack = const_cast<AVPacket*>(packet);
	AVPacket output_packet;
    av_init_packet(&output_packet);

    int e = 0;
    if (bsf_ctx_) {
	  e = av_bsf_send_packet(bsf_ctx_, parsed_pack);

	  e = av_bsf_receive_packet(bsf_ctx_, &output_packet);
    }


    UniedkVdecStream stream;
    memset(&stream, 0, sizeof(stream));

    stream.bits = output_packet.data;
    stream.len = output_packet.size;
    stream.pts = output_packet.pts;
    bool ret = true;
    if (UniedkVdecSendStream(vdec_, &stream, 5000) != 0) {
      LOG(ERROR) << "EasyDK Samples] [EasyDecodeImpl] FeedPacket(): Send stream failed";
      ret = false;
    }
    // free packet
    if (e >= 0 && bsf_ctx_) {
      av_freep(&parsed_pack->data);
    }
    return ret;
  }
  void FeedEos() override {
    UniedkVdecStream stream;
    stream.bits = nullptr;
    stream.len = 0;
    stream.pts = 0;
    UniedkVdecSendStream(vdec_, &stream, 5000);
  }
  void ReleaseFrame(UniedkBufSurface* surf) override {
    UniedkBufSurfaceDestroy(surf);
  }

  void Destroy() {
    if (bsf_ctx_) {
      av_bsf_free(&bsf_ctx_);
      bsf_ctx_ = nullptr;
    }
    if (vdec_) {
      UniedkVdecDestroy(vdec_);
      vdec_ = nullptr;
    }

    if (surf_pool_) {
      UniedkBufPoolDestroy(surf_pool_);
      surf_pool_ = nullptr;
    }
  }

  static int GetBufSurface_(UniedkBufSurface **surf,
                            int width, int height, UniedkBufSurfaceColorFormat fmt,
                            int timeout_ms, void*userdata) {
    EasyDecodeImpl *thiz =  reinterpret_cast<EasyDecodeImpl*>(userdata);
    return thiz->GetBufSurface(surf, width, height, fmt, timeout_ms);
  }

  static int OnFrame_(UniedkBufSurface *surf, void *userdata) {
    EasyDecodeImpl *thiz =  reinterpret_cast<EasyDecodeImpl*>(userdata);
    return thiz->OnFrame(surf);
  }

  static int OnDecodeEos_(void *userdata) {
    EasyDecodeImpl *thiz = reinterpret_cast<EasyDecodeImpl*>(userdata);
    return thiz->OnDecodeEos();
  }

  static int OnError_(int errCode, void *userdata) {
    EasyDecodeImpl *thiz = reinterpret_cast<EasyDecodeImpl*>(userdata);
    return thiz->OnError(errCode);
  }

  int GetBufSurface(UniedkBufSurface **surf,
                    int width, int height, UniedkBufSurfaceColorFormat fmt,
                    int timeout_ms) {
    int count = timeout_ms + 1;
    int retry_cnt = 1;
    while (1) {
      int ret = UniedkBufSurfaceCreateFromPool(surf, surf_pool_);
      if (ret == 0) {
        return 0;
      }
      count -= retry_cnt;
      VLOG(3) << "EasyDK Samples] [EasyDecodeImpl] GetBufSurface(): retry, remaining times: " << count;
      if (count <= 0) {
        LOG(ERROR) << "EasyDK Samples] [EasyDecodeImpl] GetBufSurface(): Maximum number of attempts reached: "
                   << timeout_ms;
        return -1;
      }

      usleep(1000 * retry_cnt);
      retry_cnt = std::min(retry_cnt * 2, 10);
    }
    return 0;
  }

  int OnFrame(UniedkBufSurface *surf) {
    handle_->OnDecodeFrame(surf);
    return 0;
  }

  int OnDecodeEos() {
    handle_->OnDecodeEos();
    return 0;
  }

  int OnError(int errCode) {
    return 0;
  }

  ~EasyDecodeImpl() {
    EasyDecodeImpl::Destroy();
  }

 private:
  const  AVBitStreamFilter *bsf_ = nullptr;
  AVBSFContext *bsf_ctx_ = nullptr;
  void* vdec_{nullptr};
  void* surf_pool_ = nullptr;
};

VideoDecoder::VideoDecoder(StreamRunner* runner, DecoderType type, int device_id) : runner_(runner) {
  switch (type) {
    case EASY_DECODE:
      impl_ = new EasyDecodeImpl(this, runner, device_id);
      break;
    default:
      LOG(FATAL) << "[EasyDK Samples] [VideoDecoder] Unsupported decoder type";
  }
}

VideoDecoder::~VideoDecoder() {
  if (impl_) delete impl_;
}

bool VideoDecoder::OnParseInfo(const VideoInfo& info) {
  info_ = info;
  return impl_->Init();
}

bool VideoDecoder::OnPacket(const AVPacket* packet) {
  return impl_->FeedPacket(packet);
}

void VideoDecoder::OnEos() {
  if (send_eos_ == false) {
    LOG(INFO) << "[EasyDK Samples] [VideoDecoder] OnEos(): Feed EOS";
    impl_->FeedEos();
    send_eos_ = true;
  }
}

bool VideoDecoder::Running() {
  return runner_->Running();
}

void VideoDecoder::Destroy() {
  impl_->Destroy();
}
