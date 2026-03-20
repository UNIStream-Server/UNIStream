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

#ifndef MPS_SERVICE_HPP_
#define MPS_SERVICE_HPP_

#include <map>
#include <memory>
#include <mutex>
#include <tuple>
#include <utility>
#include <vector>

#include "axcl_vdec.h"
#include "axcl_venc.h"


#include "uniedk_buf_surface.h"
#include "uniedk_encode.h"

namespace uniedk {

//
// vpps grp management
//
constexpr int KVppsVencBase = 8;
constexpr int KVppsAppBase = 12;

struct VBInfo {
  int block_size = 0;
  int block_count = 0;
  explicit VBInfo(int block_size = 0, int block_count = 0) : block_size(block_size), block_count(block_count) {}
};

struct MpsServiceConfig {
  struct {
    bool enable = false;
    AX_IMG_FORMAT_E input_fmt = AX_FORMAT_YUV420_SEMIPLANAR;
    int max_input_width = 1920;
    int max_input_height = 1080;
  } vout;
  std::vector<VBInfo> vbs;
  int codec_id_start = 0;
};

class IVBInfo {
 public:
  virtual ~IVBInfo() {}
  virtual void OnVBInfo(AX_U64 blkSize, int blkCount) = 0;
};

class IVDecResult {
 public:
  virtual ~IVDecResult() {}
  virtual void OnFrame(void *handle, const AX_VIDEO_FRAME_INFO_T *info) = 0;
  virtual void OnEos() = 0;
  virtual void OnError(AX_S32 errcode) = 0;
};

struct VEncFrameBits {
  unsigned char *bits = nullptr;
  int len = 0;
  uint64_t pts = -1;
  UniedkVencPakageType pkt_type;
};

class IVEncResult {
 public:
  virtual ~IVEncResult() {}
  virtual void OnFrameBits(VEncFrameBits *framebits) = 0;
  virtual void OnEos() = 0;
  virtual void OnError(AX_S32 errcode) = 0;
};

struct VencCreateParam {
  int gop_size;
  double frame_rate;
  AX_IMG_FORMAT_E pixel_format;
  AX_PAYLOAD_TYPE_E type;
  uint32_t width;
  uint32_t height;
  uint32_t bitrate;  // kbps
};

class NonCopyable {
 public:
  NonCopyable() = default;
  virtual ~NonCopyable() = default;

 private:
  NonCopyable(const NonCopyable &) = delete;
  NonCopyable &operator=(const NonCopyable &) = delete;
  NonCopyable(NonCopyable &&) = delete;
  NonCopyable &operator=(NonCopyable &&) = delete;
};

class MpsServiceImpl;
class MpsService {
 public:
  static MpsService &Instance() {
    static MpsService mps;
    return mps;
  }

  int Init(const MpsServiceConfig &config);
  void Destroy();

  // vdecs
  void *CreateVDec(IVDecResult *result, AX_PAYLOAD_TYPE_E type, int max_width, int max_height, int buf_num = 12,
                   AX_IMG_FORMAT_E pix_fmt = AX_FORMAT_YUV420_SEMIPLANAR);
  int DestroyVDec(void *handle);
  int VDecSendStream(void *handle, const AX_VDEC_STREAM_T *pst_stream, AX_S32 milli_sec);
  int VDecReleaseFrame(void *handle, const AX_VIDEO_FRAME_INFO_T *info);

  // vencs
  void *CreateVEnc(IVEncResult *result, VencCreateParam *params);
  int DestroyVEnc(void *handle);
  int VEncSendFrame(void *handle, const AX_VIDEO_FRAME_INFO_T *pst_frame, AX_S32 milli_sec);

  // vgu resize-convert,
  // TODO(gaoyujia): add constraints
  int VguScaleCsc(const AX_VIDEO_FRAME_INFO_T *src, AX_VIDEO_FRAME_INFO_T *dst);

  //
  #if 0
  // draw bboxes, etc
  int OsdDrawBboxes(const AX_VIDEO_FRAME_INFO_T *info, const std::vector<std::tuple<Bbox, AX_U32, AX_U32>> &bboxes);
  int OsdFillBboxes(const AX_VIDEO_FRAME_INFO_T *info, const std::vector<std::pair<Bbox, AX_U32>> &bboxes);
  int OsdPutText(const AX_VIDEO_FRAME_INFO_T *info, const std::vector<std::tuple<Bbox, void *, AX_U32, AX_U32>> &texts);
  #endif

 private:
  MpsService();
  ~MpsService();

  MpsService(const MpsService &) = delete;
  MpsService(MpsService &&) = delete;
  MpsService &operator=(const MpsService &) = delete;
  MpsService &operator=(MpsService &&) = delete;

 private:
  std::unique_ptr<MpsServiceImpl> impl_;
};

}  // namespace uniedk

#endif  // MPS_SERVICE_HPP_
