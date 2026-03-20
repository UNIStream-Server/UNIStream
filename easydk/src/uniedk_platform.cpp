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

#include "uniedk_platform.h"

#include <cstring>  // for memset
#include <map>
#include <mutex>
#include <string>

#include "glog/logging.h"
#include "ax650/ax_service/ax_service.hpp"
#include "axcl.h"

#ifdef PLATFORM_CE3226
#include "ce3226/mps_service/mps_service.hpp"
#endif
#include "common/utils.hpp"

namespace uniedk {

struct DeviceInfo {
  std::string prop_name;
  bool support_unified_addr;
  bool can_map_host_memory;
};

static std::mutex gDeviceInfoMutex;
static std::map<int, DeviceInfo> gDeviceInfoMap;

static int GetDeviceInfo(int device_id, DeviceInfo *info) {
  std::unique_lock<std::mutex> lk(gDeviceInfoMutex);
  if (gDeviceInfoMap.count(device_id)) {
    *info = gDeviceInfoMap[device_id];
    return 0;
  }
	
  unsigned int count;
  UNIDRV_SAFECALL(axclrtGetDeviceCount(&count), "GetDeviceInfo(): failed", -1);

  if (device_id > static_cast<int>(count) || device_id < 0) {
    LOG(ERROR) << "[EasyDK] GetDeviceInfo(): device id is invalid, device_id: " << device_id << ", total count: "
               << count;
    return -1;
  }
  //axclrtSetDevice(device_id);

  DeviceInfo dev_info;
  const char *pcName = axclrtGetSocName();
  if (NULL == pcName)
  {
    LOG(ERROR) << "[EasyDK] GetDeviceInfo(): soc name is null";
  }

  VLOG(2) << "[EasyDK] GetDeviceInfo(): soc name: " << pcName;
  dev_info.prop_name = pcName;

  if (dev_info.prop_name == "AX650N") {
  	dev_info.support_unified_addr = true;
	dev_info.can_map_host_memory = false;
  }

  *info = dev_info;
  gDeviceInfoMap[device_id] = dev_info;
  return 0;
}

#ifdef PLATFORM_AX650N
int PlatformInitAX650(UniedkPlatformConfig *config) {
  MpsServiceConfig mps_config;
  if (config->sensor_num > 0) {
    for (int i = 0; i < config->sensor_num; i++) {
      UniedkSensorParams *params = &config->sensor_params[i];
      int blk_size = params->out_width * params->out_height * 2;
      blk_size = (blk_size + 1023) / 1024 * 1024;
      mps_config.vbs.push_back(VBInfo(blk_size, 6));
    }
  }

  if (config->vout_params) {
    mps_config.vout.enable = true;
    mps_config.vout.input_fmt = AX_FORMAT_YUV420_SEMIPLANAR;
    mps_config.vout.max_input_width = config->vout_params->max_input_width;
    mps_config.vout.max_input_height = config->vout_params->max_input_height;
    int blk_size = mps_config.vout.max_input_height * mps_config.vout.max_input_width * 2;
    blk_size = (blk_size + 1023) / 1024 * 1024;
    mps_config.vbs.push_back(VBInfo(blk_size, 6));
  }

  // FIXME
  if (config->codec_id_start < 0 || config->codec_id_start >= 16) {
    return -1;
  }
  mps_config.codec_id_start = config->codec_id_start;

  return MpsService::Instance().Init(mps_config);
}
#endif

}  // namespace uniedk

#ifdef __cplusplus
extern "C" {
#endif

int UniedkPlatformInit(UniedkPlatformConfig *config) {
  // TODO(gaoyujia)
  unsigned int count;
  UNIDRV_SAFECALL(axclInit(NULL), "UniedkPlatformInit(): failed", -1);
  UNIDRV_SAFECALL(axclrtGetDeviceCount(&count), "UniedkPlatformInit(): failed", -1);
  axclrtDeviceList stDeviceList;
  UNIDRV_SAFECALL(axclrtGetDeviceList(&stDeviceList), "UniedkPlatformInit(): failed", -1);

  for (int i = 0; i < static_cast<int>(stDeviceList.num); i++) {
    uniedk::DeviceInfo dev_info;
    if (uniedk::GetDeviceInfo(i, &dev_info) < 0) {
      LOG(ERROR) << "[EasyDK] UniedkPlatformInit(): Get device information failed";
      return -1;
    }
  }

  UNIDRV_SAFECALL(axclrtSetDevice(stDeviceList.devices[0]), "UniedkPlatformInit(): failed", -1);

  UNIDRV_SAFECALL(axclrtEngineInit(AXCL_VNPU_DISABLE), "axclrtEngineInit(): failed", -1);

#ifdef PLATFORM_AX650N
  // FIXME
  if (stDeviceList.num == 1) {
    uniedk::DeviceInfo dev_info;
    uniedk::GetDeviceInfo(0, &dev_info);

    if (dev_info.prop_name == "AX650N") {
      return uniedk::PlatformInitAX650(config);
    }
  }
#endif

  return -1;
}

int UniedkPlatformUninit() {
#ifdef PLATFORM_AX650N
  uniedk::MpsService::Instance().Destroy();
#endif
  return 0;
}

int UniedkPlatformGetInfo(int device_id, UniedkPlatformInfo *info) {
  uniedk::DeviceInfo dev_info;
  if (uniedk::GetDeviceInfo(device_id, &dev_info) < 0) {
    return -1;
  }
  memset(info, 0, sizeof(UniedkPlatformInfo));
  snprintf(info->name, sizeof(info->name), "%s", dev_info.prop_name.c_str());
  info->can_map_host_memory = (dev_info.can_map_host_memory == true);
  info->support_unified_addr = (dev_info.support_unified_addr == true);
  return 0;
}

int UniedkCreateThdContext(int deviceId) {
  int ret = 0;
  axclrtContext context = NULL;
  axclrtDeviceList deviceList;

  ret = axclrtGetCurrentContext(&context);
  if (0 == ret)
  {
    return 0;
  }
  
  UNIDRV_SAFECALL(axclrtGetDeviceList(&deviceList), "UniedkCreateThdContext(): failed", -1);
  if (deviceId >= (int)deviceList.num)
  {
    LOG(ERROR) << "[UniedkCreateThdContext] device_id " << deviceId << "error";
	return -1;
  }
  
  UNIDRV_SAFECALL(axclrtCreateContext(&context, deviceList.devices[deviceId]), "UniedkCreateThdContext(): failed", -1);
  return 0;
}

int UniedkDestroyThdContext(void) {
  int ret = 0;
  axclrtContext context = NULL;
  ret = axclrtGetCurrentContext(&context);
  if (0 != ret)
  {
    return 0;
  }
  
  UNIDRV_SAFECALL(axclrtDestroyContext(context), "UniedkCreateThdContext(): failed", -1);
  
  return 0;
}

#ifdef __cplusplus
}
#endif
