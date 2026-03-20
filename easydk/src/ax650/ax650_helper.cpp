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
#include "ax650_helper.hpp"

#include <cstring>  // for memset

#include "mps_config.h"
#include "glog/logging.h"
#include "axcl_rt.h"
#include "../common/utils.hpp"

namespace uniedk {

UniedkBufSurfaceColorFormat GetSurfFmt(AX_IMG_FORMAT_E fmt) {
  switch (fmt) {
    case AX_FORMAT_YUV420_SEMIPLANAR:
      return UNIEDK_BUF_COLOR_FORMAT_NV12;
    case AX_FORMAT_YUV420_SEMIPLANAR_VU:
      return UNIEDK_BUF_COLOR_FORMAT_NV21;
    default:
      break;
  }
  return UNIEDK_BUF_COLOR_FORMAT_INVALID;
}

static AX_IMG_FORMAT_E GetMpsFmt(UniedkBufSurfaceColorFormat fmt) {
  switch (fmt) {
    case UNIEDK_BUF_COLOR_FORMAT_NV12:
      return AX_FORMAT_YUV420_SEMIPLANAR;
    case UNIEDK_BUF_COLOR_FORMAT_NV21:
      return AX_FORMAT_YUV420_SEMIPLANAR_VU;
    case UNIEDK_BUF_COLOR_FORMAT_BGR:
      return AX_FORMAT_BGR888;
    case UNIEDK_BUF_COLOR_FORMAT_ARGB1555:
      return AX_FORMAT_ARGB1555;
    default:
      break;
  }
  return AX_FORMAT_INVALID;
}
int BufSurfaceToVideoFrameInfo(UniedkBufSurface *surf, AX_VIDEO_FRAME_INFO_T *info, int surfIndex) {
  UniedkBufSurfaceParams *params = &surf->surface_list[surfIndex];
  uint64_t vir_addr = (uint64_t)params->mapped_data_ptr;
  uint64_t phy_addr = (uint64_t)params->data_ptr;
  AX_IMG_FORMAT_E fmt = GetMpsFmt(params->color_format);
  if (fmt == AX_FORMAT_INVALID) {
    LOG(ERROR) << "[EasyDK] BufSurfaceToVideoFrameInfo(): Unsupported pixel format: " << params->color_format;
    return -1;
  }

  if (surf->mem_type != UNIEDK_BUF_MEM_VB && surf->mem_type != UNIEDK_BUF_MEM_VB_CACHED && 
  	  surf->mem_type != UNIEDK_BUF_MEM_UNIFIED) {
    LOG(ERROR) << "[EasyDK] BufSurfaceToVideoFrameInfo(): surf must be allocated with mem_type VB, cur type " 
		       << surf->mem_type;
    return -1;
  }

  memset(info, 0, sizeof(AX_VIDEO_FRAME_INFO_T));
  info->enModId = AX_ID_USER;
  //info->u64PoolId = static_cast<uint64_t>(pool_id);
  info->stVFrame.u64PTS = surf->pts;
  info->stVFrame.enImgFormat = GetMpsFmt(params->color_format);
  info->stVFrame.u32Width = params->width;
  info->stVFrame.u32Height = params->height;
  for (uint32_t i = 0; i < params->plane_params.num_planes; i++) {
    info->stVFrame.u32PicStride[i] = params->plane_params.pitch[i];
    info->stVFrame.u64VirAddr[i] = vir_addr;
    info->stVFrame.u64PhyAddr[i] = phy_addr;
    vir_addr += params->plane_params.psize[i];
    phy_addr += params->plane_params.psize[i];
  }
  return 0;
}

}  // namespace uniedk
