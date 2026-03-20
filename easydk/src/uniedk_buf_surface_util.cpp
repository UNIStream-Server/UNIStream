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
#include "uniedk_buf_surface_util.hpp"

#include <unistd.h>

#include <algorithm>
#include <memory>
#include <thread>

#include "glog/logging.h"
#include "axcl_rt.h"
#include "common/utils.hpp"

namespace uniedk {
//
// BufSurfaceWrapper
//
UniedkBufSurface *BufSurfaceWrapper::GetBufSurface() const {
  std::unique_lock<std::mutex> lk(mutex_);
  return surf_;
}

UniedkBufSurface *BufSurfaceWrapper::BufSurfaceChown() {
  std::unique_lock<std::mutex> lk(mutex_);
  UniedkBufSurface *surf = surf_;
  surf_ = nullptr;
  return surf;
}

UniedkBufSurfaceParams *BufSurfaceWrapper::GetSurfaceParams(uint32_t batch_idx) const {
  std::unique_lock<std::mutex> lk(mutex_);
  return &surf_->surface_list[batch_idx];
}

uint32_t BufSurfaceWrapper::GetNumFilled() const {
  std::unique_lock<std::mutex> lk(mutex_);
  return surf_->num_filled;
}

UniedkBufSurfaceColorFormat BufSurfaceWrapper::GetColorFormat() const {
  std::unique_lock<std::mutex> lk(mutex_);
  return GetSurfaceParamsPriv(0)->color_format;
}

uint32_t BufSurfaceWrapper::GetWidth() const {
  std::unique_lock<std::mutex> lk(mutex_);
  return GetSurfaceParamsPriv(0)->width;
}

uint32_t BufSurfaceWrapper::GetHeight() const {
  std::unique_lock<std::mutex> lk(mutex_);
  return GetSurfaceParamsPriv(0)->height;
}

uint32_t BufSurfaceWrapper::GetStride(uint32_t i) const {
  std::unique_lock<std::mutex> lk(mutex_);
  UniedkBufSurfacePlaneParams *params = &(GetSurfaceParamsPriv(0)->plane_params);
  if (i < 0 || i >= params->num_planes) {
    LOG(ERROR) << "[EasyDK] [BufSurfaceWrapper] GetStride(): plane index is invalid.";
    return 0;
  }
  return params->pitch[i];
}

uint32_t BufSurfaceWrapper::GetPlaneNum() const {
  std::unique_lock<std::mutex> lk(mutex_);
  UniedkBufSurfacePlaneParams *params = &(GetSurfaceParamsPriv(0)->plane_params);
  return params->num_planes;
}

uint32_t BufSurfaceWrapper::GetPlaneBytes(uint32_t i) const {
  std::unique_lock<std::mutex> lk(mutex_);
  UniedkBufSurfacePlaneParams *params = &(GetSurfaceParamsPriv(0)->plane_params);
  if (i < 0 || i >= params->num_planes) {
    LOG(ERROR) << "[EasyDK] [BufSurfaceWrapper] GetPlaneBytes(): plane index is invalid.";
    return 0;
  }
  return params->psize[i];
}

int BufSurfaceWrapper::GetDeviceId() const {
  std::unique_lock<std::mutex> lk(mutex_);
  return surf_->device_id;
}

UniedkBufSurfaceMemType BufSurfaceWrapper::GetMemType() const {
  std::unique_lock<std::mutex> lk(mutex_);
  return surf_->mem_type;
}


void *BufSurfaceWrapper::GetData(uint32_t plane_idx, uint32_t batch_idx) {
  std::unique_lock<std::mutex> lk(mutex_);
  UniedkBufSurfaceParams *params = GetSurfaceParamsPriv(batch_idx);
  unsigned char *addr = static_cast<unsigned char *>(params->data_ptr);
  return static_cast<void *>(addr + params->plane_params.offset[plane_idx]);
}

void *BufSurfaceWrapper::GetMappedData(uint32_t plane_idx, uint32_t batch_idx) {
  std::unique_lock<std::mutex> lk(mutex_);
  UniedkBufSurfaceParams *params = GetSurfaceParamsPriv(batch_idx);
  unsigned char *addr = static_cast<unsigned char *>(params->mapped_data_ptr);
  return static_cast<void *>(addr + params->plane_params.offset[plane_idx]);
}

uint64_t BufSurfaceWrapper::GetPts() const {
  std::unique_lock<std::mutex> lk(mutex_);
  if (surf_) {
    return surf_->pts;
  } else {
    return pts_;
  }
}

void BufSurfaceWrapper::SetPts(uint64_t pts) {
  std::unique_lock<std::mutex> lk(mutex_);
  if (surf_) {
    surf_->pts = pts;
  } else {
    pts_ = pts;
  }
}
void *BufSurfaceWrapper::GetHostData(uint32_t plane_idx, uint32_t batch_idx) {
  std::unique_lock<std::mutex> lk(mutex_);
  UniedkBufSurfaceParams *params = GetSurfaceParamsPriv(batch_idx);
  //printf("GetHostData mem_type[%d] data_ptr[%p] mapped_data_ptr[%p]\n", surf_->mem_type, params->data_ptr, params->mapped_data_ptr);
  unsigned char *addr = static_cast<unsigned char *>(params->mapped_data_ptr);
  if (surf_->mem_type == UNIEDK_BUF_MEM_PINNED || surf_->mem_type == UNIEDK_BUF_MEM_SYSTEM) {
    addr = static_cast<unsigned char *>(params->data_ptr);
  }

  if (addr) {
  	if (surf_->mem_type == UNIEDK_BUF_MEM_DEVICE || surf_->mem_type == UNIEDK_BUF_MEM_VB_CACHED || 
  	  surf_->mem_type == UNIEDK_BUF_MEM_UNIFIED_CACHED) {
      UniedkBufSurfaceSyncForCpu(surf_, batch_idx, plane_idx);
    }
    return static_cast<void *>(addr + params->plane_params.offset[plane_idx]);
  }

  // workaround, FIXME,  copy data from device to host here
  if (surf_->mem_type == UNIEDK_BUF_MEM_DEVICE || surf_->mem_type == UNIEDK_BUF_MEM_VB_CACHED || 
  	  surf_->mem_type == UNIEDK_BUF_MEM_UNIFIED_CACHED) {
    if (surf_->is_contiguous) {
      size_t total_size = surf_->batch_size * params->data_size;
      host_data_[0].reset(new unsigned char[(total_size + 63) / 64 * 64]);
	  CALL_UNIRT_FUNC(axclrtMemcpy(host_data_[0].get(), surf_->surface_list[0].data_ptr, total_size,
                                AXCL_MEMCPY_DEVICE_TO_HOST),
                     "[BufSurfaceWrapper] GetHostData(): data is contiguous, copy data D2H failed");
      for (size_t i = 0; i < surf_->batch_size; i++) {
        GetSurfaceParamsPriv(i)->mapped_data_ptr = host_data_[0].get() + i * params->data_size;
      }
      addr = static_cast<unsigned char *>(params->mapped_data_ptr);
      return static_cast<void *>(addr + params->plane_params.offset[plane_idx]);
    } else {
      if (batch_idx >= 128) {
        LOG(ERROR) << "[EasyDK] [BufSurfaceWrapper] GetHostData(): batch index should not be greater than 128";
        return nullptr;
      }
      host_data_[batch_idx].reset(new unsigned char[(GetSurfaceParamsPriv(batch_idx)->data_size + 63) / 64 * 64]);
      CALL_UNIRT_FUNC(axclrtMemcpy(host_data_[batch_idx].get(), GetSurfaceParamsPriv(batch_idx)->data_ptr,
                                GetSurfaceParamsPriv(batch_idx)->data_size, AXCL_MEMCPY_DEVICE_TO_HOST),
                     "[BufSurfaceWrapper] GetHostData(): copy data D2H failed, batch_idx = " +
                     std::to_string(batch_idx));
      GetSurfaceParamsPriv(batch_idx)->mapped_data_ptr = host_data_[batch_idx].get();
      addr = static_cast<unsigned char *>(params->mapped_data_ptr);
      return static_cast<void *>(addr + params->plane_params.offset[plane_idx]);
    }
  }
  LOG(ERROR) << "[EasyDK] [BufSurfaceWrapper] GetHostData(): Unsupported memory type";
  return nullptr;
}

void BufSurfaceWrapper::SyncHostToDevice(uint32_t plane_idx, uint32_t batch_idx) {
  if (surf_->mem_type == UNIEDK_BUF_MEM_DEVICE) {
    if (batch_idx >= 0 && batch_idx < 128 && host_data_[batch_idx]) {
      CALL_UNIRT_FUNC(axclrtMemcpy(surf_->surface_list[batch_idx].data_ptr, host_data_[batch_idx].get(),
                                surf_->surface_list[batch_idx].data_size, AXCL_MEMCPY_HOST_TO_DEVICE),
                     "[BufSurfaceWrapper] SyncHostToDevice(): copy data H2D failed, batch_idx = " +
                     std::to_string(batch_idx));
      return;
    }

    if (batch_idx == (uint32_t)(-1)) {
      if (surf_->is_contiguous) {
        if (!host_data_[0]) {
          LOG(ERROR) << "[EasyDK] [BufSurfaceWrapper] SyncHostToDevice(): Host data is null";
          return;
        }
        size_t total_size = surf_->batch_size * GetSurfaceParamsPriv(0)->data_size;
        CALL_UNIRT_FUNC(axclrtMemcpy(surf_->surface_list[0].data_ptr, host_data_[0].get(),
                                  total_size, AXCL_MEMCPY_HOST_TO_DEVICE),
                       "[BufSurfaceWrapper] SyncHostToDevice(): data is contiguous, copy data H2D failed");
      } else {
        if (surf_->batch_size >= 128) {
          LOG(ERROR) << "[EasyDK] [BufSurfaceWrapper] SyncHostToDevice: batch size should not be greater than 128,"
                     << " which is: " << surf_->batch_size;
          return;
        }
        for (uint32_t i = 0; i < surf_->batch_size; i++) {
          CALL_UNIRT_FUNC(axclrtMemcpy(surf_->surface_list[i].data_ptr, host_data_[i].get(),
                                    surf_->surface_list[i].data_size, AXCL_MEMCPY_HOST_TO_DEVICE),
                         "[BufSurfaceWrapper] SyncHostToDevice(): copy data H2D failed, batch_idx = " +
                         std::to_string(batch_idx));
        }
      }
    }
    return;
  }
  UniedkBufSurfaceSyncForDevice(surf_, batch_idx, plane_idx);
}
//
// BufPool
//
int BufPool::CreatePool(UniedkBufSurfaceCreateParams *params, uint32_t block_count) {
  std::unique_lock<std::mutex> lk(mutex_);

  int ret = UniedkBufPoolCreate(&pool_, params, block_count);
  if (ret != 0) {
    LOG(ERROR) << "[EasyDK] [BufPool] CreatePool(): Create BufSurface pool failed";
    return -1;
  }

  stopped_ = false;
  VLOG(2) << "[EasyDK] [BufPool] CreatePool(): Done";
  return 0;
}

void BufPool::DestroyPool(int timeout_ms) {
  std::unique_lock<std::mutex> lk(mutex_);
  if (stopped_) {
    LOG(INFO) << "[EasyDK] [BufPool] DestroyPool(): Pool has been stooped";
    return;
  }
  stopped_ = true;
  int count = timeout_ms + 1;
  int retry_cnt = 1;
  while (1) {
    if (pool_) {
      int ret;
      ret = UniedkBufPoolDestroy(pool_);
      if (ret == 0) { return; }

      count -= retry_cnt;
      VLOG(3) << "[EasyDK] [BufPool] DestroyPool(): retry, remaining times: " << count;
      if (count <= 0) {
        LOG(ERROR) << "[EasyDK] [BufPool] DestroyPool(): Maximum number of attempts reached: " << timeout_ms;
        return;
      }

      lk.unlock();
      usleep(1000 * retry_cnt);
      retry_cnt = std::min(retry_cnt * 2, 10);
      lk.lock();
    }
    return;
  }
}

BufSurfWrapperPtr BufPool::GetBufSurfaceWrapper(int timeout_ms) {
  std::unique_lock<std::mutex> lk(mutex_);
  if (!pool_) {
    LOG(ERROR) << "[EasyDK] [BufPool] GetBufSurfaceWrapper(): Pool is not created";
    return nullptr;
  }

  UniedkBufSurface *surf = nullptr;
  int count = timeout_ms + 1;
  int retry_cnt = 1;
  while (1) {
    if (stopped_) {
      // Destroy called, disable alloc-new-block
      LOG(ERROR) << "[EasyDK] [BufPool] GetBufSurfaceWrapper(): Pool is stopped";
      return nullptr;
    }

    int ret = UniedkBufSurfaceCreateFromPool(&surf, pool_);
    if (ret == 0) {
      return std::make_shared<BufSurfaceWrapper>(surf);
    }
    count -= retry_cnt;
    VLOG(3) << "[EasyDK] [BufPool] GetBufSurfaceWrapper(): retry, remaining times: " << count;
    if (count <= 0) {
      LOG(ERROR) << "[EasyDK] [BufPool] GetBufSurfaceWrapper(): Maximum number of attempts reached: " << timeout_ms;
      return nullptr;
    }

    lk.unlock();
    usleep(1000 * retry_cnt);
    retry_cnt = std::min(retry_cnt * 2, 10);
    lk.lock();
  }
  return nullptr;
}

}  // namespace uniedk
