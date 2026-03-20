/*************************************************************************
 * Copyright (C) [2020] by Cambricon, Inc. All rights reserved
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

#include "video_parser.h"

#include <glog/logging.h>
#include <atomic>
#include <chrono>
#include <thread>


#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#ifdef __cplusplus
}
#endif
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

namespace detail {
static int InterruptCallBack(void* ctx) {
  VideoParser* parser = reinterpret_cast<VideoParser*>(ctx);
  if (parser->CheckTimeout()) {
    VLOG(4) << "[EasyDK Samples] [VideoParser] Rtsp: Get interrupt and timeout";
    return 1;
  }
  return 0;
}
}  // namespace detail

bool VideoParser::CheckTimeout() {
  std::chrono::duration<float, std::milli> dura = std::chrono::steady_clock::now() - last_receive_frame_time_;
  if (dura.count() > max_receive_timeout_) {
    return true;
  }
  return false;
}

bool VideoParser::Open(const char *url, bool save_file) {
  static struct _InitFFmpeg {
    _InitFFmpeg() {
      // init ffmpeg
      avformat_network_init();
    }
  } _init_ffmpeg;

  is_rtsp_ = ::IsRtsp(url);
  if (have_video_source_.load()) return false;
  // format context
  p_format_ctx_ = avformat_alloc_context();
  if (!p_format_ctx_) return false;
  if (is_rtsp_) {
    AVIOInterruptCB intrpt_callback = {detail::InterruptCallBack, this};
    p_format_ctx_->interrupt_callback = intrpt_callback;
    last_receive_frame_time_ = std::chrono::steady_clock::now();
    // options
    av_dict_set(&options_, "buffer_size", "1024000", 0);
    av_dict_set(&options_, "max_delay", "500000", 0);
    av_dict_set(&options_, "stimeout", "20000000", 0);
    av_dict_set(&options_, "rtsp_flags", "prefer_tcp", 0);
  } else {
    av_dict_set(&options_, "buffer_size", "1024000", 0);
    av_dict_set(&options_, "stimeout", "200000", 0);
  }
  // open input
  int ret_code = avformat_open_input(&p_format_ctx_, url, NULL, &options_);
  if (0 != ret_code) {
    LOG(ERROR) << "[EasyDK Samples] [VideoParser] Open(): Can not open input stream: " << url;
    return false;
  }
  // find video stream information
  ret_code = avformat_find_stream_info(p_format_ctx_, NULL);
  if (ret_code < 0) {
    LOG(ERROR) << "[EasyDK Samples] [VideoParser] Open(): Can not find stream information.";
    return false;
  }
  video_index_ = -1;
  AVStream *vstream = nullptr;
  for (uint32_t iloop = 0; iloop < p_format_ctx_->nb_streams; iloop++) {
    vstream = p_format_ctx_->streams[iloop];
  #if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
    if (vstream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
  #else
    if (vstream->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
  #endif
      video_index_ = iloop;
      break;
    }
  }
  if (video_index_ == -1) {
    LOG(ERROR) << "[EasyDK Samples] [VideoParser] Open(): Can not find a video stream.";
    return false;
  }

  #if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
  info_.width = vstream->codecpar->width;
  info_.height = vstream->codecpar->height;
  #else
  info_.width = vstream->codec->width;
  info_.height = vstream->codec->height;
  #endif

  // Get codec id, check progressive
#if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
  auto codec_id = vstream->codecpar->codec_id;
  int field_order = vstream->codecpar->field_order;
#else
  auto codec_id = vstream->codec->codec_id;
  int field_order = vstream->codec->field_order;
#endif
  info_.codec_id = codec_id;
#if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
  info_.codecpar = p_format_ctx_->streams[video_index_]->codecpar;
#endif

  /*
   * At this moment, if the demuxer does not set this value (avctx->field_order == UNKNOWN),
   * the input stream will be assumed as progressive one.
   */
  switch (field_order) {
    case AV_FIELD_TT:
    case AV_FIELD_BB:
    case AV_FIELD_TB:
    case AV_FIELD_BT:
      info_.progressive = 0;
      break;
    case AV_FIELD_PROGRESSIVE:  // fall through
    default:
      info_.progressive = 1;
      break;
  }

  // get extra data
#if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
  uint8_t* extradata = vstream->codecpar->extradata;
  int extradata_size = vstream->codecpar->extradata_size;
#else
  uint8_t* extradata = vstream->codec->extradata;
  int extradata_size = vstream->codec->extradata_size;
#endif
  info_.extra_data = std::vector<uint8_t>(extradata, extradata + extradata_size);

  LOG(INFO) << "[EasyDK Samples] [VideoParser] Open(): Format name is " << p_format_ctx_->iformat->name;
  if (strstr(p_format_ctx_->iformat->name, "mp4") || strstr(p_format_ctx_->iformat->name, "flv") ||
      strstr(p_format_ctx_->iformat->name, "matroska") || strstr(p_format_ctx_->iformat->name, "h264") ||
      strstr(p_format_ctx_->iformat->name, "rtsp")) {
    if (AV_CODEC_ID_H264 == codec_id) {
      // info_.codec_type = edk::CodecType::H264;
      if (save_file) saver_.reset(new detail::FileSaver("out.h264"));
    } else if (AV_CODEC_ID_HEVC == codec_id) {
      // info_.codec_type = edk::CodecType::H265;
      if (save_file) saver_.reset(new detail::FileSaver("out.h265"));
    } else {
      LOG(ERROR) << "[EasyDK Samples] [VideoParser] Open(): Unsupported codec id: " << codec_id;
      return false;
    }
  }
  
  av_init_packet(&packet_);
  pkt_filtered_.data = nullptr;
  pkt_filtered_.size = 0;
  
  have_video_source_.store(true);
  first_frame_ = true;

  if (AV_CODEC_ID_H264 == codec_id || AV_CODEC_ID_HEVC == codec_id)
  {
      #if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
      const AVBitStreamFilter *bsf = av_bsf_get_by_name((AV_CODEC_ID_H264 == codec_id) ? "h264_mp4toannexb" : "hevc_mp4toannexb");
	  if (!bsf) {
	  	LOG(ERROR) << "[EasyDK Samples] [VideoParser] Open(): av_bsf_get_by_name fail.";
	  }

	  int err = av_bsf_alloc(bsf, &bsfc_);
      if (err < 0) {
        LOG(ERROR) << "FFmpeg error: " << __FILE__ << " " << __LINE__
                       << " " << "av_bsf_alloc() failed";
        return false;
      }

	  avcodec_parameters_copy(bsfc_->par_in, info_.codecpar);

	  av_bsf_init(bsfc_);
	  #else
	  bsfc_ = av_bitstream_filter_init((AV_CODEC_ID_H264 == context->info.video.payload) ? "h264_mp4toannexb" : "hevc_mp4toannexb");
	  #endif
  }

  if (handler_ && !handler_->OnParseInfo(info_)) {
    return false;
  }
  return true;
}

void VideoParser::Close() {
  if (!have_video_source_.load()) return;
  LOG(INFO) << "[EasyDK Samples] [VideoParser] Close(): Clear FFMpeg resources";
  if (p_format_ctx_) {
    avformat_close_input(&p_format_ctx_);
    avformat_free_context(p_format_ctx_);
    av_dict_free(&options_);
    p_format_ctx_ = nullptr;
    options_ = nullptr;
  }

  if (bsfc_) {
	#if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
    av_bsf_free(&bsfc_);
	#else
    av_bitstream_filter_close(bsfc_);
	#endif
    bsfc_ = nullptr;
  }

  if (pkt_filtered_.data) {
#if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
      av_packet_unref(&pkt_filtered_);
#else
      av_freep(&pkt_filtered_.data);
      av_free_packet(&pkt_filtered_);
#endif
  }
  
  have_video_source_.store(false);
  frame_index_ = 0;
  saver_.reset();
  handler_->Destroy();
}

int VideoParser::ParseLoop(uint32_t frame_interval) {
  auto now_time = std::chrono::steady_clock::now();
  auto last_time = std::chrono::steady_clock::now();
  std::chrono::duration<double, std::milli> dura;
  int e = 0;
  
  while (handler_->Running()) {
    if (!have_video_source_.load()) {
      LOG(ERROR) << "[EasyDK Samples] [VideoParser] ParseLoop(): Video source has not been init";
      return -1;
    }

    if (av_read_frame(p_format_ctx_, &packet_) < 0) {
      // EOS
      handler_->OnEos();
      return 1;
    }

    // update receive frame time
    last_receive_frame_time_ = std::chrono::steady_clock::now();

    // skip unmatched stream
    if (packet_.stream_index != video_index_) {
      av_packet_unref(&packet_);
      continue;
    }

	if (AV_CODEC_ID_H264 == info_.codec_id || AV_CODEC_ID_HEVC == info_.codec_id)
	{
	  if (bsfc_) {
#if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
        e = av_bsf_send_packet(bsfc_, &packet_);
        if (e < 0) {
          LOG(ERROR) << "FFmpeg error: " << __FILE__ << " " << __LINE__
                    << " " << "Packet submission for filtering failed";
		  av_packet_unref(&packet_);
          continue;
        }

        e = av_bsf_receive_packet(bsfc_, &pkt_filtered_);
        if (e < 0) {
          LOG(ERROR) << "FFmpeg error: " << __FILE__ << " " << __LINE__
                    << " " << "Filter packet receive failed";
		  av_packet_unref(&packet_);
          continue;
        }
#else
        e = av_bitstream_filter_filter(bsfc_, p_format_ctx_->streams[video_index_]->codec, nullptr,
                                      &pkt_filtered_.data, &pkt_filtered_.size,
                                      packet_.data, packet_.size, 0);
        if (e < 0) {
          LOG(WARNING) << "FFmpeg error: " << __FILE__ << " " << __LINE__
                       << " " << "Filter packet receive failed";
          pkt_filtered_.data = nullptr;
          pkt_filtered_.size = 0;
		  av_packet_unref(&packet_);
          continue;
        }

#endif
      }
	}

	av_packet_unref(&packet_);

    // filter non-key-frame in head
    if (first_frame_) {
      VLOG(1) << "[EasyDK Samples] [VideoParser] ParseLoop(): Check first frame";
      if (pkt_filtered_.flags & AV_PKT_FLAG_KEY) {
        first_frame_ = false;
      } else {
        LOG(WARNING) << "[EasyDK Samples] [VideoParser]ParseLoop():  Skip first not-key-frame";
        continue;
      }
    }

    // parse data from packet
    auto vstream = p_format_ctx_->streams[video_index_];
    // find pts information
    if (AV_NOPTS_VALUE == pkt_filtered_.pts) {
      VLOG(5) << "[EasyDK Samples] [VideoParser] ParseLoop(): Didn't find pts informations,"
              << " use ordered numbers instead.";
      pkt_filtered_.pts = frame_index_++;
    } else {
      pkt_filtered_.pts = av_rescale_q(pkt_filtered_.pts, vstream->time_base, {1, 90000});
    }

    if (saver_) {
      saver_->Write(reinterpret_cast<char *>(pkt_filtered_.data), pkt_filtered_.size);
    }

    if (!handler_->OnPacket(&pkt_filtered_)) return -1;

	if (AV_CODEC_ID_H264 == info_.codec_id || AV_CODEC_ID_HEVC == info_.codec_id) {
      if (pkt_filtered_.data && pkt_filtered_.size > 0) {
		#if LIBAVFORMAT_VERSION_INT >= FFMPEG_VERSION_3_1
        av_packet_unref(&pkt_filtered_);
		#else
        av_freep(&pkt_filtered_.data);
        av_free_packet(&pkt_filtered_);
		#endif
      }
	}

    // frame rate control
    if (frame_interval) {
      now_time = std::chrono::steady_clock::now();
      dura = now_time - last_time;
      if (frame_interval > dura.count()) {
        std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(frame_interval - dura.count()));
      }
      last_time = std::chrono::steady_clock::now();
    }
  }  // while (true)

  return 1;
}
