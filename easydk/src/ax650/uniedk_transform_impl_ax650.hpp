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

#ifndef UNIEDK_TRANSFORM_IMPL_CE3226_HPP_
#define UNIEDK_TRANSFORM_IMPL_CE3226_HPP_

#include <algorithm>
#include <cstring>  // for memset
#include "../uniedk_transform_impl.hpp"
#include "uniedk_transform.h"

namespace uniedk {

class TransformerAx650 : public ITransformer {
 public:
  TransformerAx650() {
    if (config_params_.compute_mode == UNIEDK_TRANSFORM_COMPUTE_DEFAULT) {
      params_.compute_mode = UNIEDK_TRANSFORM_COMPUTE_VGU;  // for AX650
    } else {
      params_.compute_mode = config_params_.compute_mode;
    }
  }
  ~TransformerAx650() = default;
  int Transform(UniedkBufSurface *src, UniedkBufSurface *dst, UniedkTransformParams *transform_params) override;

 private:
  int TransformHw(UniedkBufSurface *src, UniedkBufSurface *dst, UniedkTransformParams *transform_params);

 private:
  UniedkTransformConfigParams params_;
};

}  // namespace uniedk

#endif  // UNIEDK_TRANSFORM_IMPL_CE3226_HPP_
