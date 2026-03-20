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

#include "uniedk_transform.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <thread>

#include "axcl_rt.h"

#include "uniedk_platform.h"
#include "uniedk_transform_impl.hpp"

#ifdef PLATFORM_AX650N
#include "ax650/uniedk_transform_impl_ax650.hpp"
#endif

#include "common/utils.hpp"

namespace uniedk {

thread_local UniedkTransformConfigParams ITransformer::config_params_;

ITransformer *CreateTransformer() {
  int dev_id = -1;
  UNIDRV_SAFECALL(axclrtGetDevice(&dev_id), "CreateTransformer(): failed", nullptr);

  UniedkPlatformInfo info;
  if (UniedkPlatformGetInfo(dev_id, &info) < 0) {
    LOG(ERROR) << "[EasyDK] CreateTransformer(): Get platform information failed";
    return nullptr;
  }

// FIXME,
//  1. check prop_name ???
//  2. load so ???
#ifdef PLATFORM_AX650N
    return new TransformerAx650();
#endif

  LOG(ERROR) << "[EasyDK] CreateTransformer(): plat not support";
  return nullptr;
}

class TransformService {
 public:
  static TransformService &Instance() {
    static std::once_flag s_flag;
    std::call_once(s_flag, [&] { instance_.reset(new TransformService); });
    return *instance_;
  }
  ~TransformService() = default;

  int SetSessionParams(UniedkTransformConfigParams *config_params) {
    if (!config_params) {
      LOG(ERROR) << "[EasyDK] [TransformService] SetSessionParams(): Parameters pointer is invalid";
      return -1;
    }
    return transformer_->SetSessionParams(config_params);
  }

  int GetSessionParams(UniedkTransformConfigParams *config_params) {
    if (!config_params) {
      LOG(ERROR) << "[EasyDK] [TransformService] GetSessionParams(): Parameters pointer is invalid";
      return -1;
    }
    return transformer_->GetSessionParams(config_params);
  }

  int Transform(UniedkBufSurface *src, UniedkBufSurface *dst, UniedkTransformParams *transform_params) {
    if (!dst || !src || !transform_params) {
      LOG(ERROR) << "[EasyDK] [TransformService] Transform(): src, dst BufSurface or parameters pointer is invalid";
      return -1;
    }
    return transformer_->Transform(src, dst, transform_params);
  }

 private:
  TransformService(const TransformService &) = delete;
  TransformService(TransformService &&) = delete;
  TransformService &operator=(const TransformService &) = delete;
  TransformService &operator=(TransformService &&) = delete;
  TransformService() { transformer_.reset(CreateTransformer()); }

 private:
  std::unique_ptr<ITransformer> transformer_ = nullptr;
  static std::unique_ptr<TransformService> instance_;
};

std::unique_ptr<TransformService> TransformService::instance_;

}  // namespace uniedk

extern "C" {

int UniedkTransformSetSessionParams(UniedkTransformConfigParams *config_params) {
  return uniedk::TransformService::Instance().SetSessionParams(config_params);
}

int UniedkTransformGetSessionParams(UniedkTransformConfigParams *config_params) {
  return uniedk::TransformService::Instance().GetSessionParams(config_params);
}

int UniedkTransform(UniedkBufSurface *src, UniedkBufSurface *dst, UniedkTransformParams *transform_params) {
  return uniedk::TransformService::Instance().Transform(src, dst, transform_params);
}

};  // extern "C"
