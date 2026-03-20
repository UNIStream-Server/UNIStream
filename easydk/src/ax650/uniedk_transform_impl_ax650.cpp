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

#include "uniedk_transform_impl_ax650.hpp"

#include <algorithm>
#include <cstring>  // for memset
#include <vector>

#include "glog/logging.h"
#include "ax650_helper.hpp"
#include "uniedk_transform.hpp"
#include "ax_service/ax_service.hpp"

namespace uniedk {

int TransformerAx650::Transform(UniedkBufSurface *src, UniedkBufSurface *dst, UniedkTransformParams *transform_params) {
  if (src->num_filled > dst->batch_size) {
    LOG(ERROR) << "[EasyDK] [TransformerCe3226] Transform(): The number of inputs exceeds batch size: "
               << src->num_filled << " v.s. " << dst->batch_size;
    return -1;
  }

  if (src->mem_type != UNIEDK_BUF_MEM_VB && src->mem_type != UNIEDK_BUF_MEM_VB_CACHED &&
      src->mem_type != UNIEDK_BUF_MEM_UNIFIED && src->mem_type != UNIEDK_BUF_MEM_UNIFIED_CACHED &&
      src->mem_type != UNIEDK_BUF_MEM_DEVICE &&
      dst->mem_type != UNIEDK_BUF_MEM_VB && dst->mem_type != UNIEDK_BUF_MEM_VB_CACHED &&
      dst->mem_type != UNIEDK_BUF_MEM_UNIFIED && dst->mem_type != UNIEDK_BUF_MEM_UNIFIED_CACHED &&
      dst->mem_type != UNIEDK_BUF_MEM_DEVICE) {
    LOG(ERROR) << "[EasyDK] [TransformerCe3226] Transform(): The src and dst mem_type is invalid. "
               << "src mem_type: " << src->mem_type << ", dst_mem_type: " << dst->mem_type;
    return -1;
  }

  if (config_params_.compute_mode == UNIEDK_TRANSFORM_COMPUTE_MLU ||
      (src->mem_type != UNIEDK_BUF_MEM_VB && src->mem_type != UNIEDK_BUF_MEM_VB_CACHED) ||
      (dst->mem_type != UNIEDK_BUF_MEM_VB && dst->mem_type != UNIEDK_BUF_MEM_VB_CACHED) ||
      transform_params->transform_flag & UNIEDK_TRANSFORM_MEAN_STD) {
	if (TransformHw(src, dst, transform_params) == 0) {
      return 0;
    }
  }

  if ((config_params_.compute_mode == UNIEDK_TRANSFORM_COMPUTE_VGU ||
      config_params_.compute_mode == UNIEDK_TRANSFORM_COMPUTE_DEFAULT) &&
      (src->mem_type == UNIEDK_BUF_MEM_VB || src->mem_type == UNIEDK_BUF_MEM_VB_CACHED) &&
      (dst->mem_type == UNIEDK_BUF_MEM_VB || dst->mem_type == UNIEDK_BUF_MEM_VB_CACHED)) {
    if (TransformHw(src, dst, transform_params) == 0) {
      return 0;
    }
  }

  LOG(ERROR) << "[EasyDK] [TransformerAx650] Transform(): this transformer not support ";
  return -1;
}

int TransformerAx650::TransformHw(UniedkBufSurface *src, UniedkBufSurface *dst, UniedkTransformParams *transform_params) {
  UniedkBufSurface *dst_tmp = dst;
  UniedkBufSurface transform_dst;
  std::vector<UniedkBufSurfaceParams> dst_params;
  if (dst->surface_list[0].color_format == UNIEDK_BUF_COLOR_FORMAT_TENSOR) {
    int dst_batch_size = transform_params->dst_desc->shape.n;
    dst_params.reserve(dst_batch_size);
    memset(&transform_dst, 0, sizeof(UniedkBufSurface));
    transform_dst.surface_list = dst_params.data();

    if (GetBufSurfaceFromTensor(dst, &transform_dst, transform_params->dst_desc) < 0) {
      LOG(ERROR) << "[EasyDK] [TransformerCe3226] TransformHw(): Get BufSurface from tensor descriptions failed";
      return -1;
    }
    dst_tmp = &transform_dst;
  }

  for (uint32_t i = 0; i < src->num_filled; i++) {
    // FIXME, add more supported colorFormats in the future
    if (src->surface_list[i].color_format != UNIEDK_BUF_COLOR_FORMAT_NV12 &&
        src->surface_list[i].color_format != UNIEDK_BUF_COLOR_FORMAT_NV21) {
      VLOG(3) << "[EasyDK] [TransformerCe3226] TransformHw(): Unsupported src color format: "
              << src->surface_list[i].color_format;
      return -1;
    }
    if (dst_tmp->surface_list[i].color_format != UNIEDK_BUF_COLOR_FORMAT_BGR) {
      VLOG(3) << "[EasyDK] [TransformerCe3226] TransformHw(): Unsupported dst color format: "
              << dst_tmp->surface_list[i].color_format;
      return -1;
    }

    AX_VIDEO_FRAME_INFO_T srcInfo, dstInfo;
    if (BufSurfaceToVideoFrameInfo(src, &srcInfo, i) < 0) {
      LOG(WARNING) << "[EasyDK] [TransformerCe3226] TransformHw(): Convert src BufSurface to VideoFrameInfo failed";
      return -1;
    }
    if (BufSurfaceToVideoFrameInfo(dst_tmp, &dstInfo, i) < 0) {
      LOG(WARNING) << "[EasyDK] [TransformerCe3226] TransformHw(): Convert dst BufSurface to VideoFrameInfo failed";
      return -1;
    }

    // apply roi
    if (transform_params->transform_flag & UNIEDK_TRANSFORM_CROP_SRC) {
      UniedkTransformRect *rect = &transform_params->src_rect[i];
      // FIXME, check ROI
      srcInfo.stVFrame.u32Width = rect->width;
      srcInfo.stVFrame.u32Height = rect->height;
      srcInfo.stVFrame.u64PhyAddr[0] +=
          rect->top * src->surface_list[i].pitch + rect->left * src->surface_list[i].plane_params.bytes_per_pix[0];
      srcInfo.stVFrame.u64PhyAddr[1] +=
          rect->top / 2 * src->surface_list[i].pitch + rect->left * src->surface_list[i].plane_params.bytes_per_pix[1];
    }

    if (transform_params->transform_flag & UNIEDK_TRANSFORM_CROP_DST) {
      UniedkTransformRect *rect = &transform_params->dst_rect[i];
      // FIXME, check ROI
      dstInfo.stVFrame.u32Width = rect->width;
      dstInfo.stVFrame.u32Height = rect->height;
      dstInfo.stVFrame.u64PhyAddr[0] += rect->top * dst_tmp->surface_list[i].pitch +
          rect->left * dst_tmp->surface_list[i].plane_params.bytes_per_pix[0];
    }
    // FIXME, validate resolution, the minimum resolutoin that ce3226 supports is 64x64.
    if (srcInfo.stVFrame.u32Width < 64 || srcInfo.stVFrame.u32Height < 64) {
      LOG(WARNING) << "[EasyDK] [TransformerCe3226] TransformHw(): src resolution is not supported, smaller than 64x64";
      return -1;
    }

    if (dstInfo.stVFrame.u32Width < 64 || dstInfo.stVFrame.u32Height < 64) {
      LOG(WARNING) << "[EasyDK] [TransformerCe3226] TransformHw(): dst resolution is not supported, smaller than 64x64";
      return -1;
    }

    // do transform
    if (MpsService::Instance().VguScaleCsc(&srcInfo, &dstInfo) < 0) {
      LOG(ERROR) << "[EasyDK] [TransformerCe3226] TransformHw(): vgu scale scs failed";
      return -1;
    }
  }
  return 0;
}

}  // namespace uniedk
