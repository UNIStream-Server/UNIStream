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

#include "uniedk_buf_surface_impl_unified.h"

#include <cstdlib>  // for malloc/free
#include <cstring>  // for memset
#include <string>

#include "mps_config.h"
#include "glog/logging.h"

#include "uniedk_buf_surface_utils.h"
#include "common/utils.hpp"

#include "axcl_native.h"

namespace uniedk {

int MemAllocatorUnified::Create(UniedkBufSurfaceCreateParams *params) {
  create_params_ = *params;

  if (create_params_.batch_size == 0) {
    create_params_.batch_size = 1;
  }
  // FIXME
  uint32_t alignment_w = 16;
  uint32_t alignment_h = 2;
  if (params->force_align_1) {
    alignment_w = alignment_h = 1;
  }

  memset(&plane_params_, 0, sizeof(UniedkBufSurfacePlaneParams));
  block_size_ = params->size;

  if (!block_size_) {
    GetColorFormatInfo(params->color_format, params->width, params->height, alignment_w, alignment_h, &plane_params_);
    for (uint32_t i = 0; i < plane_params_.num_planes; i++) {
      block_size_ += plane_params_.psize[i];
    }
  } else {
    if (create_params_.color_format == UNIEDK_BUF_COLOR_FORMAT_INVALID) {
      create_params_.color_format = UNIEDK_BUF_COLOR_FORMAT_GRAY8;
    }
    block_size_ = (block_size_ + alignment_w - 1) / alignment_w * alignment_w;
    memset(&plane_params_, 0, sizeof(plane_params_));
  }
  created_ = true;
  return 0;
}

int MemAllocatorUnified::Destroy() {
  created_ = false;
  return 0;
}

int MemAllocatorUnified::Alloc(UniedkBufSurface *surf) {
  void *phy_addr, *vir_addr, *vir_addr_host;
  bool cached = create_params_.mem_type == UNIEDK_BUF_MEM_UNIFIED_CACHED;
  if (cached) {
    UNIDRV_SAFECALL(AXCL_SYS_MemAllocCached((AX_U64 *) &phy_addr, &vir_addr, block_size_ * create_params_.batch_size, 
		                             1, nullptr),
                  "[MemAllocatorUnified] Alloc(): failed", -1);
  } else {
    UNIDRV_SAFECALL(AXCL_SYS_MemAlloc((AX_U64 *) &phy_addr, &vir_addr, block_size_ * create_params_.batch_size, 
		                             1, nullptr),
                  "[MemAllocatorUnified] Alloc(): failed", -1);
  }

  vir_addr_host = reinterpret_cast<void *>(malloc(block_size_ * create_params_.batch_size));
  if (AX_NULL == vir_addr_host) {
	VLOG(3) << "[EasyDK] [MemAllocatorUnified] Alloc(): Call malloc failed, len = " << block_size_ * create_params_.batch_size ;
	return -1;
  }

  memset(surf, 0, sizeof(UniedkBufSurface));
  surf->mem_type = create_params_.mem_type;
  surf->opaque = nullptr;  // will be filled by MemPool
  surf->batch_size = create_params_.batch_size;
  surf->device_id = create_params_.device_id;
  surf->surface_list =
      reinterpret_cast<UniedkBufSurfaceParams *>(malloc(sizeof(UniedkBufSurfaceParams) * surf->batch_size));
  memset(surf->surface_list, 0, sizeof(UniedkBufSurfaceParams) * surf->batch_size);

  uint64_t iova = reinterpret_cast<uint64_t>(phy_addr);
  uint64_t uva = reinterpret_cast<uint64_t>(vir_addr);
  uint64_t uva_host = reinterpret_cast<uint64_t>(vir_addr_host);
  for (uint32_t i = 0; i < surf->batch_size; i++) {
    surf->surface_list[i].color_format = create_params_.color_format;
    surf->surface_list[i].data_ptr = reinterpret_cast<void *>(iova);
    surf->surface_list[i].vir_addr = reinterpret_cast<void *>(uva);
	surf->surface_list[i].mapped_data_ptr = reinterpret_cast<void *>(uva_host);
    surf->surface_list[i].width = create_params_.width;
    surf->surface_list[i].height = create_params_.height;
    surf->surface_list[i].pitch = plane_params_.pitch[0];
    surf->surface_list[i].data_size = block_size_;
    surf->surface_list[i].plane_params = plane_params_;
    iova += block_size_;
    uva += block_size_;
	uva_host += block_size_;
  }

  printf("unified alloc phy_addr[%p] len[%lu*%u=%lu]\n", 
  	     phy_addr, block_size_, create_params_.batch_size, block_size_ * create_params_.batch_size);
  return 0;
}

int MemAllocatorUnified::Free(UniedkBufSurface *surf) {
  void *phy_addr = surf->surface_list[0].data_ptr;
  void *vir_addr = surf->surface_list[0].vir_addr;
  
  //AXCL_SYS_Munmap(vir_addr, surf->surface_list[0].data_size * surf->batch_size);
  AXCL_SYS_MemFree((AX_U64)phy_addr, vir_addr);

  free(surf->surface_list[0].mapped_data_ptr);

  ::free(reinterpret_cast<void *>(surf->surface_list));
  return 0;
}

}  // namespace uniedk
