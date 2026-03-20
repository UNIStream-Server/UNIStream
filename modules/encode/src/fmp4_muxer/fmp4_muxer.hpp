/*************************************************************************
 * Copyright (C) [2022] by Cambricon, Inc. All rights reserved
 * Copyright (C) [2025] by UNIStream Team. All rights reserved 
 *  This file has been modified by UNIStream development team based on the original work from Cambricon, Inc.
 *  The original work is licensed under the Apache License, Version 2.0
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

#ifndef MODULES_FMP4_MUXER_HPP_
#define MODULES_FMP4_MUXER_HPP_
/*!
 *  @file mp4_muxer.hpp
 *
 *  This file contains a declaration of Mp4Muxer class.
 */
#include <memory>
#include <string>
#include <utility>

#include "uniedk_encode.h"

#include "../encode_handler.hpp"

#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#ifdef __cplusplus
}
#endif

namespace unistream {

// H264 to fragment-mp4
class Mp4Muxer {
 public:
  Mp4Muxer() = default;
  ~Mp4Muxer() = default;
  int Open(const std::string &filename, int width, int height, VideoCodecType codec_type);
  int Close();
  int Write(UniedkVEncFrameBits *framebits);

 private:
  Mp4Muxer(const Mp4Muxer &) = delete;
  Mp4Muxer(Mp4Muxer &&) = delete;
  Mp4Muxer &operator=(const Mp4Muxer &) = delete;
  Mp4Muxer &operator=(Mp4Muxer &&) = delete;

 private:
  bool header_written_ = false;
  unsigned char extradata_[2048];
  int extradata_len_ = 0;
  int64_t frame_count_ = 0;
  int64_t init_timestamp_ = 0;
  AVFormatContext *ctx_ = nullptr;
  // FILE *fp_ = nullptr;
};  // class Mp4Muxer

}  // namespace unistream

#endif  // MODULES_FMP4_MUXER_HPP_
