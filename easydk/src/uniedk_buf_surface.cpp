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
 *  You may obtain a Copy of the License at
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

#include "uniedk_buf_surface.h"

#include <algorithm>
#include <cstring>  // for memset
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "glog/logging.h"
#include "axcl_rt.h"

#include "uniedk_buf_surface_impl.h"
#include "common/utils.hpp"

namespace uniedk {

class BufSurfaceService {
 public:
  static BufSurfaceService &Instance() {
    static std::once_flag s_flag;
    std::call_once(s_flag, [&] { instance_.reset(new BufSurfaceService); });
    return *instance_;
  }
  ~BufSurfaceService() = default;
  int BufPoolCreate(void **pool, UniedkBufSurfaceCreateParams *params, uint32_t block_num) {
    if (pool && params && block_num) {
      MemPool *mempool = new MemPool();
      if (!mempool) {
        LOG(ERROR) << "[EasyDK] [BufSurfaceService] BufPoolCreate(): new memory pool failed";
        return -1;
      }
      *pool = reinterpret_cast<void *>(mempool);
      if (mempool->Create(params, block_num) == 0) {
        return 0;
      }
      delete mempool;
      LOG(ERROR) << "[EasyDK] [BufSurfaceService] BufPoolCreate(): Create memory pool failed";
      return -1;
    }
    return -1;
  }
  int BufPoolDestroy(void *pool) {
    if (pool) {
      MemPool *mempool = reinterpret_cast<MemPool *>(pool);
      if (mempool) {
        int ret = mempool->Destroy();
        if (ret != 0) {
          VLOG(3) << "[EasyDK] [BufSurfaceService] BufPoolDestroy(): Destroy memory pool failed, ret = " << ret;
          return ret;
        }
        delete mempool;
      }
      return 0;
    }
    LOG(ERROR) << "[EasyDK] [BufSurfaceService] BufPoolDestroy(): Pool is not existed";
    return -1;
  }
  int CreateFromPool(UniedkBufSurface **surf, void *pool) {
    if (surf && pool) {
      UniedkBufSurface surface;
      MemPool *mempool = reinterpret_cast<MemPool *>(pool);
      if (mempool->Alloc(&surface) < 0) {
        VLOG(4) << "[EasyDK] [BufSurfaceService] CreateFromPool(): Create BufSurface from pool failed";
        return -1;
      }
      *surf = AllocSurface();
      if (!(*surf)) {
        mempool->Free(&surface);
        LOG(WARNING) << "[EasyDK] [BufSurfaceService] CreateFromPool(): Alloc BufSurface failed";
        return -1;
      }
      *(*surf) = surface;
      return 0;
    }
    LOG(ERROR) << "[EasyDK] [BufSurfaceService] CreateFromPool(): surf or pool is nullptr";
    return -1;
  }
  int Create(UniedkBufSurface **surf, UniedkBufSurfaceCreateParams *params) {
    if (surf && params) {
      if (CheckParams(params) < 0) {
        LOG(ERROR) << "[EasyDK] [BufSurfaceService] Create(): Parameters are invalid";
        return -1;
      }
      UniedkBufSurface surface;
      if (CreateSurface(params, &surface) < 0) {
        LOG(ERROR) << "[EasyDK] [BufSurfaceService] Create(): Create BufSurface failed";
        return -1;
      }
      *surf = AllocSurface();
      if (!(*surf)) {
        DestroySurface(&surface);
        LOG(ERROR) << "[EasyDK] [BufSurfaceService] Create(): Alloc BufSurface failed";
        return -1;
      }
      *(*surf) = surface;
      return 0;
    }
    LOG(ERROR) << "[EasyDK] [BufSurfaceService] Create(): surf or params is nullptr";
    return -1;
  }

  int Destroy(UniedkBufSurface *surf) {
    if (!surf) {
      LOG(ERROR) << "[EasyDK] [BufSurfaceService] Destroy(): surf is nullptr";
      return -1;
    }

    if (surf->opaque) {
      MemPool *mempool = reinterpret_cast<MemPool *>(surf->opaque);
      int ret = mempool->Free(surf);
      FreeSurface(surf);
      if (ret) {
        LOG(ERROR) << "[EasyDK] [BufSurfaceService] Destroy(): Free BufSurface back to memory pool failed";
      }
      return ret;
    }

    // created by UniedkBufSurfaceCreate()
    int ret = DestroySurface(surf);
    FreeSurface(surf);
    if (ret) {
      LOG(ERROR) << "[EasyDK] [BufSurfaceService] Destroy(): Destroy BufSurface failed";
    }
    return ret;
  }

  int SyncForCpu(UniedkBufSurface *surf, int index, int plane) {
    if (surf->mem_type == UNIEDK_BUF_MEM_UNIFIED_CACHED || surf->mem_type == UNIEDK_BUF_MEM_VB_CACHED) {
      if (index == -1) {
        for (uint32_t i = 0; i < surf->batch_size; i++) {
		  axclrtMemcpy(surf->surface_list[i].mapped_data_ptr, surf->surface_list[i].data_ptr, 
		  	           surf->surface_list[i].data_size, AXCL_MEMCPY_DEVICE_TO_HOST);
        }
      } else {
        if (index < 0 || index >= static_cast<int>(surf->batch_size)) {
          LOG(ERROR) << "[EasyDK] [BufSurfaceService] SyncForCpu(): batch index is invalid";
          return -1;
        }

		axclrtMemcpy(surf->surface_list[index].mapped_data_ptr, surf->surface_list[index].data_ptr, 
		  	         surf->surface_list[index].data_size, AXCL_MEMCPY_DEVICE_TO_HOST);
      }
      return 0;
    }
    return -1;
  }

  int SyncForDevice(UniedkBufSurface *surf, int index, int plane) {
    if (surf->mem_type == UNIEDK_BUF_MEM_UNIFIED_CACHED || surf->mem_type == UNIEDK_BUF_MEM_VB_CACHED) {
      if (index == -1) {
        for (uint32_t i = 0; i < surf->batch_size; i++) {
		  axclrtMemcpy(surf->surface_list[i].data_ptr, surf->surface_list[i].mapped_data_ptr, 
		  	           surf->surface_list[i].data_size, AXCL_MEMCPY_HOST_TO_DEVICE);
        }
      } else {
        if (index < 0 || index >= static_cast<int>(surf->batch_size)) {
          LOG(ERROR) << "[EasyDK] [BufSurfaceService] SyncForDevice(): batch index is invalid";
          return -1;
        }
		axclrtMemcpy(surf->surface_list[index].data_ptr, surf->surface_list[index].mapped_data_ptr, 
		  	         surf->surface_list[index].data_size, AXCL_MEMCPY_HOST_TO_DEVICE);
      }
      return 0;
    }
    return -1;
  }

  int Memset(UniedkBufSurface *surf, int index, int plane, uint8_t value) {
    if (!surf) {
      LOG(ERROR) << "[EasyDK] [BufSurfaceService] Memset(): BufSurface is nullptr";
      return -1;
    }
    if (index < -1 || index >= static_cast<int>(surf->batch_size)) {
      LOG(ERROR) << "[EasyDK] [BufSurfaceService] Memset(): batch index is invalid";
      return -1;
    }
    if (plane < -1 || plane >= static_cast<int>(surf->surface_list[0].plane_params.num_planes)) {
      LOG(ERROR) << "[EasyDK] [BufSurfaceService] Memset(): plane index is invalid";
      return -1;
    }
    if (surf->mem_type == UNIEDK_BUF_MEM_SYSTEM || surf->mem_type == UNIEDK_BUF_MEM_PINNED) {
      for (uint32_t i = 0; i < surf->batch_size; i++) {
        if (index >=0 && i != static_cast<uint32_t>(index)) continue;
        for (uint32_t j = 0; j < surf->surface_list[0].plane_params.num_planes; j++) {
          if (plane >= 0 && j != static_cast<uint32_t>(plane)) continue;
          unsigned char *dst8 = static_cast<unsigned char *>(surf->surface_list[i].data_ptr);
          dst8 += surf->surface_list[i].plane_params.offset[j];
          uint32_t size = surf->surface_list[i].plane_params.psize[j];
          memset(dst8, value, size);
        }
      }
      return 0;
    }
    // device memory
    for (uint32_t i = 0; i < surf->batch_size; i++) {
      if (index >=0 && i != static_cast<uint32_t>(index)) continue;
      for (uint32_t j = 0; j < surf->surface_list[0].plane_params.num_planes; j++) {
        if (plane >= 0 && j != static_cast<uint32_t>(plane)) continue;
        unsigned char *dst8 = static_cast<unsigned char *>(surf->surface_list[i].data_ptr);
        dst8 += surf->surface_list[i].plane_params.offset[j];
        uint32_t size = surf->surface_list[i].plane_params.psize[j];
        UNIDRV_SAFECALL(axclrtMemset(dst8, value, size), "[BufSurfaceService] Memset(): failed", -1);
      }
    }
    return 0;
  }

  int Copy(UniedkBufSurface *src_surf, UniedkBufSurface *dst_surf) {
    if (!src_surf || !dst_surf) {
      LOG(ERROR) << "[EasyDK] [BufSurfaceService] Copy(): src or dst BufSurface is nullptr";
      return -1;
    }
    // check parameters, must be the same size
    if (src_surf->batch_size != dst_surf->batch_size) {
      LOG(ERROR) << "[EasyDK] [BufSurfaceService] Copy(): src and dst BufSurface has different batch size";
      return -1;
    }

    dst_surf->pts = src_surf->pts;
    bool src_host = (src_surf->mem_type == UNIEDK_BUF_MEM_SYSTEM || src_surf->mem_type == UNIEDK_BUF_MEM_PINNED);

    bool dst_host = (dst_surf->mem_type == UNIEDK_BUF_MEM_SYSTEM || dst_surf->mem_type == UNIEDK_BUF_MEM_PINNED);

    if ((!dst_host && !src_host) && (src_surf->device_id != dst_surf->device_id)) {
      LOG(ERROR) << "[EasyDK] [BufSurfaceService] Copy(): src and dst BufSurface is on different device";
      return -1;
    }

    for (size_t i = 0; i < src_surf->batch_size; ++i) {
      if (src_surf->surface_list[i].data_size != dst_surf->surface_list[i].data_size) {
        uint8_t* src_data_ptr = reinterpret_cast<uint8_t*>(src_surf->surface_list[i].data_ptr);
        uint8_t* dst_data_ptr = reinterpret_cast<uint8_t*>(dst_surf->surface_list[i].data_ptr);
        uint8_t* src = src_data_ptr;
        uint8_t* dst = dst_data_ptr;

        for (uint32_t plane_idx = 0 ; plane_idx < src_surf->surface_list[i].plane_params.num_planes; plane_idx++) {
          uint32_t src_plane_offset = src_surf->surface_list[i].plane_params.offset[plane_idx];
          uint32_t dst_plane_offset = dst_surf->surface_list[i].plane_params.offset[plane_idx];
          if (plane_idx && (!src_plane_offset ||!dst_plane_offset)) {
            LOG(ERROR) << "[EasyDK] [BufSurfaceService] Copy(): src or dst BufSurface plane parameter offset is wrong";
            return -1;
          }
          uint32_t copy_size = src_surf->surface_list[i].plane_params.width[plane_idx] *
                               src_surf->surface_list[i].plane_params.bytes_per_pix[plane_idx];
          uint32_t src_step = src_surf->surface_list[i].plane_params.pitch[plane_idx];
          uint32_t dst_step = dst_surf->surface_list[i].plane_params.pitch[plane_idx];

          if (!copy_size || !src_step || !dst_step) {
            LOG(ERROR) << "[EasyDK] [BufSurfaceService] Copy(): src or dst BufSurface plane parameter width, pitch"
                       << " or bytes_per_pix is wrong";
            return -1;
          }
          for (uint32_t h_idx = 0; h_idx < src_surf->surface_list[i].plane_params.height[i]; h_idx++) {
            if (dst_host && src_host) {
              UNIDRV_SAFECALL(axclrtMemcpy(dst, src, copy_size, AXCL_MEMCPY_HOST_TO_HOST),
                            "[BufSurfaceService] Copy(): failed", -1);
            } else if (dst_host && !src_host) {
              UNIDRV_SAFECALL(axclrtMemcpy(dst, src, copy_size, AXCL_MEMCPY_DEVICE_TO_HOST),
                            "[BufSurfaceService] Copy(): failed", -1);
            } else if (!dst_host && src_host) {
              UNIDRV_SAFECALL(axclrtMemcpy(dst, src, copy_size, AXCL_MEMCPY_HOST_TO_DEVICE),
                            "[BufSurfaceService] Copy(): failed", -1);
            } else {
              UNIDRV_SAFECALL(axclrtMemcpy(dst, src, copy_size, AXCL_MEMCPY_DEVICE_TO_DEVICE),
                            "[BufSurfaceService] Copy(): failed", -1);
            }
            src += src_step;
            dst += dst_step;
          }
          src = src_data_ptr + src_plane_offset;
          dst = dst_data_ptr + dst_plane_offset;
        }
      } else {
        if (dst_host && src_host) {
		  UNIDRV_SAFECALL(axclrtMemcpy(dst_surf->surface_list[i].data_ptr, src_surf->surface_list[i].data_ptr, 
		  	                         dst_surf->surface_list[i].data_size, AXCL_MEMCPY_HOST_TO_HOST),
                        "[BufSurfaceService] Copy(): failed", -1);
		  
        } else if (dst_host && !src_host) {
		  UNIDRV_SAFECALL(axclrtMemcpy(dst_surf->surface_list[i].data_ptr, src_surf->surface_list[i].data_ptr, 
		  	                         dst_surf->surface_list[i].data_size, AXCL_MEMCPY_DEVICE_TO_HOST),
                        "[BufSurfaceService] Copy(): failed", -1);
        } else if (!dst_host && src_host) {
		  UNIDRV_SAFECALL(axclrtMemcpy(dst_surf->surface_list[i].data_ptr, src_surf->surface_list[i].data_ptr, 
		  	                         dst_surf->surface_list[i].data_size, AXCL_MEMCPY_HOST_TO_DEVICE),
                        "[BufSurfaceService] Copy(): failed", -1);
        } else {
		  UNIDRV_SAFECALL(axclrtMemcpy(dst_surf->surface_list[i].data_ptr, src_surf->surface_list[i].data_ptr, 
		  	                         dst_surf->surface_list[i].data_size, AXCL_MEMCPY_DEVICE_TO_DEVICE),
                        "[BufSurfaceService] Copy(): failed", -1);
        }
      }
    }

    //
    bool dstCached =
        (dst_surf->mem_type == UNIEDK_BUF_MEM_UNIFIED_CACHED || dst_surf->mem_type == UNIEDK_BUF_MEM_VB_CACHED);
    if (dstCached) {
      SyncForCpu(dst_surf, -1, -1);
    }
    return 0;
  }

 private:
  BufSurfaceService(const BufSurfaceService &) = delete;
  BufSurfaceService(BufSurfaceService &&) = delete;
  BufSurfaceService &operator=(const BufSurfaceService &) = delete;
  BufSurfaceService &operator=(BufSurfaceService &&) = delete;
  BufSurfaceService() = default;

 private:
  std::mutex mutex_;
  bool initialized_ = false;
  std::queue<UniedkBufSurface *> surfpool_;
  UniedkBufSurface *start_ = nullptr, *end_ = nullptr;
  static const int k_surfs_num_ = 256 * 1024;

 private:
  void CreateSurfsPool() {
    if (initialized_) return;
    start_ = reinterpret_cast<UniedkBufSurface *>(malloc(sizeof(UniedkBufSurface) * k_surfs_num_));
    if (!start_) {
      LOG(ERROR) << "[EasyDK] [BufSurfaceService] CreateSurfsPool(): Create BufSurface pointers failed";
      return;
    }
    end_ = &start_[k_surfs_num_ - 1];
    for (int i = 0; i < k_surfs_num_; i++) surfpool_.push(&start_[i]);
    initialized_ = true;
  }

  UniedkBufSurface *AllocSurface() {
    std::unique_lock<std::mutex> lk(mutex_);
    if (!initialized_) CreateSurfsPool();
    if (!surfpool_.empty()) {
      UniedkBufSurface *res = surfpool_.front();
      surfpool_.pop();
      return res;
    }
    return reinterpret_cast<UniedkBufSurface *>(malloc(sizeof(UniedkBufSurface)));
  }

  void FreeSurface(UniedkBufSurface *surf) {
    std::unique_lock<std::mutex> lk(mutex_);
    if (surf >= start_ && surf <= end_) {
      surfpool_.push(surf);
      return;
    }
    ::free(surf);
  }

 private:
  static std::unique_ptr<BufSurfaceService> instance_;
};

std::unique_ptr<BufSurfaceService> BufSurfaceService::instance_;

}  // namespace uniedk

extern "C" {
int UniedkBufPoolCreate(void **pool, UniedkBufSurfaceCreateParams *params, uint32_t block_num) {
  return uniedk::BufSurfaceService::Instance().BufPoolCreate(pool, params, block_num);
}

int UniedkBufPoolDestroy(void *pool) { return uniedk::BufSurfaceService::Instance().BufPoolDestroy(pool); }

int UniedkBufSurfaceCreateFromPool(UniedkBufSurface **surf, void *pool) {
  return uniedk::BufSurfaceService::Instance().CreateFromPool(surf, pool);
}

int UniedkBufSurfaceCreate(UniedkBufSurface **surf, UniedkBufSurfaceCreateParams *params) {
  return uniedk::BufSurfaceService::Instance().Create(surf, params);
}

int UniedkBufSurfaceDestroy(UniedkBufSurface *surf) { return uniedk::BufSurfaceService::Instance().Destroy(surf); }

int UniedkBufSurfaceSyncForCpu(UniedkBufSurface *surf, int index, int plane) {
  return uniedk::BufSurfaceService::Instance().SyncForCpu(surf, index, plane);
}

int UniedkBufSurfaceSyncForDevice(UniedkBufSurface *surf, int index, int plane) {
  return uniedk::BufSurfaceService::Instance().SyncForDevice(surf, index, plane);
}

int UniedkBufSurfaceMemSet(UniedkBufSurface *surf, int index, int plane, uint8_t value) {
  return uniedk::BufSurfaceService::Instance().Memset(surf, index, plane, value);
}

int UniedkBufSurfaceCopy(UniedkBufSurface *src_surf, UniedkBufSurface *dst_surf) {
  return uniedk::BufSurfaceService::Instance().Copy(src_surf, dst_surf);
}

};  // extern "C"
