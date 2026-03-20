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

#include "uniedk_buf_surface_impl_vb.h"

#include <unistd.h>
#include <cstdlib>  // for malloc/free
#include <cstring>  // for memset
#include <string>
#include <thread>

#include "glog/logging.h"

#include "uniedk_buf_surface_utils.h"
#include "common/utils.hpp"
#include "ax_pool_type.h"
#include "axcl_native.h"

namespace uniedk {

int MemAllocatorVb::Create(UniedkBufSurfaceCreateParams *params) {
  create_params_ = *params;
  // FIXME
  uint32_t alignment_w = 16;
  uint32_t alignment_h = 2;
  if (params->force_align_1) {
    alignment_w = alignment_h = 1;
  }

  if (create_params_.color_format == UNIEDK_BUF_COLOR_FORMAT_INVALID) {
    create_params_.color_format = UNIEDK_BUF_COLOR_FORMAT_GRAY8;
  }

  bool cached = create_params_.mem_type == UNIEDK_BUF_MEM_VB_CACHED;
  memset(&plane_params_, 0, sizeof(UniedkBufSurfacePlaneParams));
  block_size_ = params->size;

  if (block_size_) {
    block_size_ = (block_size_ + alignment_w - 1) / alignment_w * alignment_w;
  } else {
    GetColorFormatInfo(params->color_format, params->width, params->height, alignment_w, alignment_h, &plane_params_);
    for (uint32_t i = 0; i < plane_params_.num_planes; i++) {
      block_size_ += plane_params_.psize[i];
    }
  }

  AX_POOL_CONFIG_T PoolConfig;
  memset(&PoolConfig, 0, sizeof(PoolConfig));
  PoolConfig.BlkCnt = block_num_;
  PoolConfig.BlkSize = block_size_ * create_params_.batch_size;
  PoolConfig.MetaSize = 4096;
  PoolConfig.CacheMode = cached ? POOL_CACHE_MODE_CACHED : POOL_CACHE_MODE_NONCACHE;
  pool_id_ = AXCL_POOL_CreatePool(&PoolConfig);

  return 0;
}

int MemAllocatorVb::Destroy() {

  auto ret = AXCL_POOL_DestroyPool(pool_id_);
  if (ret != AXCL_SUCC) {
    VLOG(3) << "[EasyDK] [MemAllocatorVb] Destroy(): Call AXCL_POOL_DestroyPool failed, pool id = " << pool_id_
            << ", ret = " << ret;
    return -1;
  }

  created_ = false;
  return 0;
}

int MemAllocatorVb::Alloc(UniedkBufSurface *surf) {
  AX_U64 phy_addr, vir_addr;
  AX_BLK vb_handle;
  
  vb_handle = AXCL_POOL_GetBlock(pool_id_, block_size_, AX_NULL);
  if (AX_INVALID_BLOCKID == vb_handle) {
    VLOG(3) << "[EasyDK] [MemAllocatorVb] Alloc(): Call AXCL_POOL_GetBlock failed, pool id = " << pool_id_
            << ", vb_handle = " << vb_handle;
    return -1;
  }

  phy_addr = AXCL_POOL_Handle2PhysAddr(vb_handle);
  if (AX_NULL == phy_addr) {
	VLOG(3) << "[EasyDK] [MemAllocatorVb] Alloc(): Call AXCL_POOL_Handle2PhysAddr failed, pool id = " << pool_id_
			<< ", phy_addr = " << phy_addr;
	return -1;
  }

  vir_addr = reinterpret_cast<AX_U64>(malloc(block_size_));
  if (AX_NULL == phy_addr) {
	VLOG(3) << "[EasyDK] [MemAllocatorVb] Alloc(): Call malloc failed, len = " << block_size_ ;
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

  uint64_t iova = static_cast<uint64_t>(phy_addr);
  uint64_t uva = static_cast<uint64_t>(vir_addr);
  for (uint32_t i = 0; i < surf->batch_size; i++) {
    surf->surface_list[i].color_format = create_params_.color_format;
    surf->surface_list[i].data_ptr = reinterpret_cast<void *>(iova);
	surf->surface_list[i].mapped_data_ptr = reinterpret_cast<void *>(uva);
    surf->surface_list[i].width = create_params_.width;
    surf->surface_list[i].height = create_params_.height;
    surf->surface_list[i].pitch = plane_params_.pitch[0];
    surf->surface_list[i].data_size = block_size_;
    surf->surface_list[i].plane_params = plane_params_;
    iova += block_size_;
	uva += block_size_;
  }

  printf("VB alloc phy_addr[%p] block_size_[%lu]\n", (void *)phy_addr, block_size_);
  return 0;
}

int MemAllocatorVb::Free(UniedkBufSurface *surf) {
  uint64_t phy_addr;
  phy_addr = reinterpret_cast<uint64_t>(surf->surface_list[0].data_ptr);

  AX_BLK vb_handle;
  vb_handle = AXCL_POOL_PhysAddr2Handle(phy_addr);
  AXCL_POOL_ReleaseBlock(vb_handle);

  free(surf->surface_list[0].mapped_data_ptr);
  
  ::free(reinterpret_cast<void *>(surf->surface_list));
  return 0;
}

}  // namespace uniedk
