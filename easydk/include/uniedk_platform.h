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

#ifndef UNIEDK_PLATFORM_H_
#define UNIEDK_PLATFORM_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Holds the parameters of a sensor.
 */
typedef struct UniedkSensorParams {
  /** The sensor type */
  int sensor_type;
  /** The mipi device */
  int mipi_dev;
  /** The bus id */
  int bus_id;
  /** The sns clock id */
  int sns_clk_id;
  /** The width of the output */
  int out_width;
  /** The height of the output */
  int out_height;
  /** Not used (NV12 by default). The color format of the output. */
  int output_format;
} UniedkSensorParams;

/**
 * Holds the parameters of vout.
 */
typedef struct UniedkVoutParams {
  /** The max width of the input */
  int max_input_width;
  /** The max height of the input */
  int max_input_height;
  /** Not used (NV12 by default). The color format of the input. */
  int input_format;
} UniedkVoutParams;

/**
 * Holds the configurations of the platform.
 */
typedef struct UniedkPlatformConfig {
  /** The number of sensors. Only Valid on CE3226 platform */
  int sensor_num;
  /** The parameters of sensors. Only Valid on CE3226 platform */
  UniedkSensorParams *sensor_params;
  /** The parameters of vout. Only Valid on CE3226 platform */
  UniedkVoutParams *vout_params;
  /** The starting codec id. Only Valid on CE3226 platform */
  int codec_id_start;
} UniedkPlatformConfig;

#define UNIEDK_PLATFORM_NAME_LEN  128
/**
 * Holds the Information of the platform.
 */
typedef struct UniedkPlatformInfo {
  /** The name of the platform */
  char name[UNIEDK_PLATFORM_NAME_LEN];
  /** Whether supporting unified address. 1 means supporting, and 0 means not supporting*/
  int support_unified_addr;
  /** Whether supporting map host memory. 1 means supporting, and 0 means not supporting*/
  int can_map_host_memory;
} UniedkPlatformInfo;

/**
 * @brief Initializes the platform. On CE3226 platform, vin and vout will be initailzed.
 *
 * @param[in] config The configurations of the platform.
 *
 * @return Returns 0 if this function has run successfully. Otherwise returns -1.
 */
int UniedkPlatformInit(UniedkPlatformConfig *config);
/**
 * @brief UnInitializes the platform. On CE3226 platform, vin and vout will be uninitailzed.
 *
 * @return Returns 0 if this function has run successfully. Otherwise returns -1.
 */
int UniedkPlatformUninit();
/**
 * @brief Gets the information of the platform.
 *
 * @param[in] device_id Specified the device id.
 * @param[out] info The information of the platform, including the name of the platfrom and so on.
 *
 * @return Returns 0 if this function has run successfully. Otherwise returns -1.
 */
int UniedkPlatformGetInfo(int device_id, UniedkPlatformInfo *info);

/**
 * @brief create the context of the thread.
 *
 * @param[in] device_id Specified the device id.
 *
 * @return Returns 0 if this function has run successfully. Otherwise returns -1.
 */
int UniedkCreateThdContext(int deviceId);

/**
 * @brief destroy the context of the thread.
 *
 *
 * @return Returns 0 if this function has run successfully. Otherwise returns -1.
 */
int UniedkDestroyThdContext(void);

#ifdef __cplusplus
}
#endif

#endif  // UNIEDK_PLATFORM_H_
