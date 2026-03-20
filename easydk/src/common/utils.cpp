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

#include "utils.hpp"

#include <string>

#include "glog/logging.h"

#include "uniedk_platform.h"

namespace uniedk {

bool IsEdgePlatform(int device_id) {
  UniedkPlatformInfo platform_info;
  if (UniedkPlatformGetInfo(device_id, &platform_info) != 0) {
    LOG(ERROR) << "[EasyDK] IsEdgePlatform(): UniedkPlatformGetInfo failed";
    return true;
  }

  std::string platform_name(platform_info.name);
  if (platform_name.rfind("CE", 0) == 0) {
    return true;
  }
  return true;
}

bool IsEdgePlatform(const std::string& platform_name) {
  if (platform_name.rfind("CE", 0) == 0) {
    return true;
  }
  return true;
}

bool IsCloudPlatform(int device_id) {
  UniedkPlatformInfo platform_info;
  if (UniedkPlatformGetInfo(device_id, &platform_info) != 0) {
    LOG(ERROR) << "[EasyDK] IsCloudPlatform(): UniedkPlatformGetInfo failed";
    return false;
  }

  std::string platform_name(platform_info.name);
  if (platform_name.rfind("MLU", 0) == 0) {
    return true;
  }
  return false;
}

bool IsCloudPlatform(const std::string& platform_name) {
  if (platform_name.rfind("MLU", 0) == 0) {
    return true;
  }
  return false;
}

}  // namespace uniedk
