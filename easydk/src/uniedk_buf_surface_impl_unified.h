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

#ifndef UNIEDK_BUF_SURFACE_IMPL_UNIFIED_H_
#define UNIEDK_BUF_SURFACE_IMPL_UNIFIED_H_

#include <string>
#include <mutex>

#include "uniedk_buf_surface_impl.h"

namespace uniedk {

class MemAllocatorUnified : public IMemAllcator {
 public:
  MemAllocatorUnified() = default;
  ~MemAllocatorUnified() = default;
  int Create(UniedkBufSurfaceCreateParams *params) override;
  int Destroy() override;
  int Alloc(UniedkBufSurface *surf) override;
  int Free(UniedkBufSurface *surf) override;

 private:
  bool created_ = false;
  UniedkBufSurfaceCreateParams create_params_;
  UniedkBufSurfacePlaneParams plane_params_;
  size_t block_size_;
};

}  // namespace uniedk

#endif  // UNIEDK_BUF_SURFACE_IMPL_UNIFIED_H_
