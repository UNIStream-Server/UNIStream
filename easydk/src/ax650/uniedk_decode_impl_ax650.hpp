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

#ifndef UNIEDK_DECODE_IMPL_AX650_HPP_
#define UNIEDK_DECODE_IMPL_AX650_HPP_

#include <atomic>
#include <future>
#include <map>
#include <memory>

#include "../uniedk_decode_impl.hpp"
#include "ax_service/ax_service.hpp"


namespace uniedk {

class DecoderAX650 : public IDecoder, public IVDecResult {
 public:
  DecoderAX650() = default;
  ~DecoderAX650() = default;
  // IDecoder
  int Create(UniedkVdecCreateParams *params) override;
  int Destroy() override;
  int SendStream(const UniedkVdecStream *stream, int timeout_ms) override;

  // IVDecResult
  void OnFrame(void *handle, const AX_VIDEO_FRAME_INFO_T *info) override;
  void OnEos() override;
  void OnError(int errcode) override;

 private:
  UniedkVdecCreateParams create_params_;
  void *vdec_ = nullptr;
};

}  // namespace uniedk

#endif  // UNIEDK_DECODE_IMPL_MLU370_HPP_

