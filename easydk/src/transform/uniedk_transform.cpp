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

#include "uniedk_transform.hpp"

#include <atomic>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "glog/logging.h"
#include "../common/utils.hpp"

namespace uniedk {

static UniedkBufSurfaceColorFormat GetColorFormatFromTensor(UniedkTransformColorFormat format) {
  static std::map<UniedkTransformColorFormat, UniedkBufSurfaceColorFormat> color_map{
      {UNIEDK_TRANSFORM_COLOR_FORMAT_BGR, UNIEDK_BUF_COLOR_FORMAT_BGR},
      {UNIEDK_TRANSFORM_COLOR_FORMAT_RGB, UNIEDK_BUF_COLOR_FORMAT_RGB},
      {UNIEDK_TRANSFORM_COLOR_FORMAT_BGRA, UNIEDK_BUF_COLOR_FORMAT_BGRA},
      {UNIEDK_TRANSFORM_COLOR_FORMAT_RGBA, UNIEDK_BUF_COLOR_FORMAT_RGBA},
      {UNIEDK_TRANSFORM_COLOR_FORMAT_ABGR, UNIEDK_BUF_COLOR_FORMAT_ABGR},
      {UNIEDK_TRANSFORM_COLOR_FORMAT_ARGB, UNIEDK_BUF_COLOR_FORMAT_ARGB},
  };
  if (color_map.find(format) != color_map.end()) return color_map[format];
  return UNIEDK_BUF_COLOR_FORMAT_LAST;
}

#if 0
static int GetChannelNumFromColor(const UniedkBufSurfaceColorFormat &corlor_format) {
  if (corlor_format == UNIEDK_BUF_COLOR_FORMAT_RGB || corlor_format == UNIEDK_BUF_COLOR_FORMAT_BGR) {
    return 3;
  } else if (corlor_format == UNIEDK_BUF_COLOR_FORMAT_BGRA || corlor_format == UNIEDK_BUF_COLOR_FORMAT_RGBA ||
             corlor_format == UNIEDK_BUF_COLOR_FORMAT_ABGR || corlor_format == UNIEDK_BUF_COLOR_FORMAT_ARGB) {
    return 4;
  }
  return -1;
}
#endif

int GetBufSurfaceFromTensor(UniedkBufSurface *src, UniedkBufSurface *dst, UniedkTransformTensorDesc *tensor_desc) {
  if (GetColorFormatFromTensor(tensor_desc->color_format) == UNIEDK_BUF_COLOR_FORMAT_LAST) return -1;

  dst->num_filled = src->num_filled;
  dst->batch_size = src->batch_size;  // tensor_desc->shape.n;
  dst->pts = src->pts;
  dst->device_id = src->device_id;
  dst->mem_type = src->mem_type;

  for (size_t i = 0; i < src->batch_size; ++i) {
    dst->surface_list[i].width = tensor_desc->shape.w;
    dst->surface_list[i].height = tensor_desc->shape.h;
    dst->surface_list[i].pitch = tensor_desc->shape.w * tensor_desc->shape.c;
    dst->surface_list[i].color_format = GetColorFormatFromTensor(tensor_desc->color_format);
    dst->surface_list[i].data_size = src->surface_list[i].data_size;
    dst->surface_list[i].data_ptr = src->surface_list[i].data_ptr;
    dst->surface_list[i].plane_params = src->surface_list[i].plane_params;
  }
  return 0;
}

bool IsYuv420sp(UniedkBufSurfaceColorFormat fmt) {
  if (fmt == UNIEDK_BUF_COLOR_FORMAT_NV12 || fmt == UNIEDK_BUF_COLOR_FORMAT_NV21) {
    return true;
  }
  return false;
}

bool IsRgbx(UniedkBufSurfaceColorFormat fmt) {
  if (fmt == UNIEDK_BUF_COLOR_FORMAT_RGB || fmt == UNIEDK_BUF_COLOR_FORMAT_BGR ||
      fmt == UNIEDK_BUF_COLOR_FORMAT_RGBA || fmt == UNIEDK_BUF_COLOR_FORMAT_BGRA ||
      fmt == UNIEDK_BUF_COLOR_FORMAT_ABGR || fmt == UNIEDK_BUF_COLOR_FORMAT_ARGB) {
    return true;
  }
  return false;
}

}  // namespace uniedk
