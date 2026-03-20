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
#ifndef MODULES_ENCODE_HANDLER_HPP_
#define MODULES_ENCODE_HANDLER_HPP_

#include <fstream>
#include <functional>
#include <memory>

#include "uniedk_encode.h"

#include "private/unistream_common_pri.hpp"
#include "unistream_frame.hpp"
#include "scaler/scaler.hpp"

namespace unistream {

class Mp4Muxer;
class RtspSink;

enum VideoCodecType {
  AUTO = -1,
  H264 = 0,
  H265,
  MPEG4,
  JPEG,
  RAW,
};


using VEncodeOnFrameBits = std::function<int(UniedkVEncFrameBits*)>;

struct VEncHandlerParam {
  uint32_t width;
  uint32_t height;
  int bitrate;
  int gop_size;
  VideoCodecType codec_type = VideoCodecType::H264;
  double frame_rate = 30;
  VEncodeOnFrameBits on_framebits = nullptr;
};


class VencHandler : private NonCopyable {
 public:
  virtual void SetParams(const VEncHandlerParam &param) { param_ = param;}

  virtual int SendFrame(std::shared_ptr<UNIFrameInfo> data) = 0;
  virtual int SendFrame(Scaler::Buffer* data) = 0;

  virtual int OnFrameBits(UniedkVEncFrameBits *framebits);

  virtual ~VencHandler() {}
 protected:
  VEncHandlerParam param_;
};

}  // namespace unistream

#endif  // __ENCODER_HANDLER_FFMPEG_HPP__
