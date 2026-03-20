/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
 * Copyright (C) [2025] by UNIStream Team. All rights reserved 
 *  This file has been modified by UNIStream development team based on the original work from Cambricon, Inc.
 *  The original work is licensed under the Apache License, Version 2.0
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

#include "test_base.hpp"

#include <string.h>
#include <unistd.h>

#include <cerrno>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "uniedk_platform.h"
#include "unistream_logging.hpp"

extern int errno;

std::string GetExePath() {
  char path[PATH_MAX_LENGTH];
  int cnt = readlink("/proc/self/exe", path, PATH_MAX_LENGTH);
  if (cnt < 0 || cnt >= PATH_MAX_LENGTH) {
    return "";
  }
  for (int i = cnt - 1; i >= 0; --i) {
    if ('/' == path[i]) {
      path[i + 1] = '\0';
      break;
    }
  }
  std::string result(path);
  return result;
}

bool IsEdgePlatform(int device_id) {
  UniedkPlatformInfo platform_info;
  if (UniedkPlatformGetInfo(device_id, &platform_info) != 0) {
    LOG(ERROR) << "[EasyDK] IsEdgePlatform(): UniedkPlatformGetInfo failed";
    return false;
  }

  std::string platform_name(platform_info.name);
  if (platform_name.rfind("CE", 0) == 0 || platform_name.rfind("AX650", 0) == 0) {
    return true;
  }
  return false;
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

int InitPlatform(bool enable_vin, bool enable_vout) {
  UniedkSensorParams sensor_params[4];
  memset(sensor_params, 0, sizeof(UniedkSensorParams) * 4);

  UniedkPlatformConfig config;
  memset(&config, 0, sizeof(config));

  config.codec_id_start = 0;

  UniedkVoutParams vout_params;
  memset(&vout_params, 0, sizeof(UniedkVoutParams));

  if (enable_vout) {
    config.vout_params = &vout_params;
    vout_params.max_input_width = 1920;
    vout_params.max_input_height = 1080;
    vout_params.input_format = 0;  // not used at the moment
  }

  if (enable_vin) {
    config.sensor_num = 1;
    config.sensor_params = sensor_params;
    sensor_params[0].sensor_type = 6;
    sensor_params[0].mipi_dev = 1;
    sensor_params[0].bus_id = 0;
    sensor_params[0].sns_clk_id = 1;
    sensor_params[0].out_width = 1920;
    sensor_params[0].out_height = 1080;
    sensor_params[0].output_format = 0;  // not used at the moment
  }
  UniedkPlatformInit(&config);
  return 0;
}

void CheckExePath(const std::string& path) {
  if (path.size() == 0) {
    LOGF_IF(UNITEST, 0 != errno) << std::string(strerror(errno));
    LOGF(UNITEST) << "length of exe path is larger than " << PATH_MAX_LENGTH;
  }
}


#define ALIGN(w, a) ((w + a - 1) & ~(a - 1))
bool CvtBgrToYuv420sp(const cv::Mat &bgr_image, uint32_t alignment, UniedkBufSurface *surf) {
  cv::Mat yuv_i420_image;
  uint32_t width, height, stride;
  uint8_t *src_y, *src_u, *src_v, *dst_y, *dst_u;
  // uint8_t *dst_v;

  cv::cvtColor(bgr_image, yuv_i420_image, cv::COLOR_BGR2YUV_I420);

  width = bgr_image.cols;
  height = bgr_image.rows;
  if (alignment > 0)
    stride = ALIGN(width, alignment);
  else
    stride = width;

  uint32_t y_len = stride * height;
  src_y = yuv_i420_image.data;
  src_u = yuv_i420_image.data + y_len;
  src_v = yuv_i420_image.data + y_len * 5 / 4;

  if (surf->mem_type == UNIEDK_BUF_MEM_VB_CACHED || surf->mem_type == UNIEDK_BUF_MEM_VB) {
    dst_y = reinterpret_cast<uint8_t *>(surf->surface_list[0].mapped_data_ptr);
    dst_u = reinterpret_cast<uint8_t *>(reinterpret_cast<uint64_t>(surf->surface_list[0].mapped_data_ptr) + y_len);
    // dst_v = reinterpret_cast<uint8_t*>(reinterpret_cast<uint64_t>(surf->surface_list[0].data_ptr) +
    //                                                               stride * height * 5 / 4);
  } else {
    dst_y = reinterpret_cast<uint8_t *>(surf->surface_list[0].data_ptr);
    dst_u = reinterpret_cast<uint8_t *>(reinterpret_cast<uint64_t>(surf->surface_list[0].data_ptr) + y_len);
  }
  for (uint32_t i = 0; i < height; i++) {
    // y data
    memcpy(dst_y + i * stride, src_y + i * width, width);
    // uv data
    if (i % 2 == 0) {
      for (uint32_t j = 0; j < width / 2; j++) {
        if (surf->surface_list->color_format == UNIEDK_BUF_COLOR_FORMAT_NV21) {
          *(dst_u + i * stride / 2 + 2 * j) = *(src_v + i * width / 4 + j);
          *(dst_u + i * stride / 2 + 2 * j + 1) = *(src_u + i * width / 4 + j);
        } else {
          *(dst_u + i * stride / 2 + 2 * j) = *(src_u + i * width / 4 + j);
          *(dst_u + i * stride / 2 + 2 * j + 1) = *(src_v + i * width / 4 + j);
        }
      }
    }
  }
  return true;
}


std::shared_ptr<unistream::UNIDataFrame> GenerateUNIDataFrame(cv::Mat img, int device_id) {
  UniedkBufSurfaceCreateParams create_params;
  create_params.batch_size = 1;
  memset(&create_params, 0, sizeof(create_params));
  create_params.device_id = device_id;
  create_params.batch_size = 1;
  create_params.width = img.cols;
  create_params.height = img.rows;
  create_params.color_format = UNIEDK_BUF_COLOR_FORMAT_NV21;
  create_params.mem_type = UNIEDK_BUF_MEM_DEVICE;

  UniedkBufSurface* surf;
  UniedkBufSurfaceCreate(&surf, &create_params);

  UniedkBufSurface* cpu_surf;
  create_params.mem_type = UNIEDK_BUF_MEM_SYSTEM;
  UniedkBufSurfaceCreate(&cpu_surf, &create_params);
  CvtBgrToYuv420sp(img, 0, cpu_surf);
  UniedkBufSurfaceCopy(cpu_surf, surf);
  UniedkBufSurfaceDestroy(cpu_surf);

  std::shared_ptr<unistream::UNIDataFrame> frame(new (std::nothrow) unistream::UNIDataFrame());
  frame->frame_id = 1;
  frame->buf_surf = std::make_shared<uniedk::BufSurfaceWrapper>(surf, false);

  return frame;
}

static int g_device_id = 0;

static std::string MMVersionForCe3226() {
  return "v0.13.0";
}
static std::string MMVersionForMlu370() {
  return "v0.13.0";
}
static std::string MMVersionForMluxxx() {
  return "v0.14.0";
}

const std::unordered_map<std::string, std::pair<std::string, std::string>> g_model_info = {
    {"resnet50_CE3226",
        {"resnet50_" + MMVersionForCe3226() + "_4b_rgb_uint8.magicmind",
         "http://video.cambricon.com/models/magicmind/" + MMVersionForCe3226() +
         "/resnet50_" + MMVersionForCe3226() + "_4b_rgb_uint8.magicmind"}
    },
    {"resnet50_MLU370",
        {"resnet50_v0.13.0_4b_rgb_uint8.magicmind",
         "http://video.cambricon.com/models/magicmind/" + MMVersionForMlu370() +
         "/resnet50_v0.13.0_4b_rgb_uint8.magicmind"}
    },
    {"resnet50_MLUxxx",
        {"resnet50_v0.14.0_4b_rgb_uint8.magicmind",
         "http://video.cambricon.com/models/magicmind/" + MMVersionForMluxxx() +
         "/resnet50_v0.14.0_4b_rgb_uint8.magicmind"}
    },
    {"feature_extract_CE3226",
        {"feature_extract_" + MMVersionForCe3226() + "_4b_rgb_uint8.magicmind",
         "http://video.cambricon.com/models/magicmind/" + MMVersionForCe3226() +
         "/feature_extract_" + MMVersionForCe3226() + "_4b_rgb_uint8.magicmind"}
    },
    {"feature_extract_MLU370",
        {"feature_extract_v0.13.0_4b_rgb_uint8.magicmind",
         "http://video.cambricon.com/models/magicmind/" + MMVersionForMlu370() +
         "/feature_extract_v0.13.0_4b_rgb_uint8.magicmind"}
    },
    {"feature_extract_MLUxxx",
        {"feature_extract_v0.14.0_4b_rgb_uint8.magicmind",
         "http://video.cambricon.com/models/magicmind/" + MMVersionForMluxxx() +
         "/feature_extract_v0.14.0_4b_rgb_uint8.magicmind"}
    },
    {"yolov3_CE3226",
        {"yolov3_" + MMVersionForCe3226() + "_4b_rgb_uint8.magicmind",
         "http://video.cambricon.com/models/magicmind/" + MMVersionForCe3226() +
         "/yolov3_" + MMVersionForCe3226() + "_4b_rgb_uint8.magicmind"}
    },
    {"yolov3_MLU370",
        {"yolov3_v0.13.0_4b_rgb_uint8.magicmind",
         "http://video.cambricon.com/models/magicmind/" + MMVersionForMlu370() +
         "/yolov3_v0.13.0_4b_rgb_uint8.magicmind"}
    },
    {"yolov3_MLUxxx",
        {"yolov3_v0.14.0_4b_rgb_uint8.magicmind",
         "http://video.cambricon.com/models/magicmind/" + MMVersionForMluxxx() +
         "/yolov3_v0.14.0_4b_rgb_uint8.magicmind"}
    }
};

std::string GetModelInfoStr(std::string model_name, std::string info_type) {
  UniedkPlatformInfo platform_info;
  UniedkPlatformGetInfo(g_device_id, &platform_info);
  std::string platform_name(platform_info.name);
  std::string model_key;
  if (platform_name.rfind("MLU5", 0) == 0) {
    model_key = model_name + "_MLUxxx";
  } else {
    model_key = model_name + "_" + platform_name;
  }
  if (g_model_info.find(model_key) != g_model_info.end()) {
    if (info_type == "name") {
      return g_model_info.at(model_key).first;
    } else {
      return g_model_info.at(model_key).second;
    }
  }
  return "";
}

const std::unordered_map<std::string, std::pair<std::string, std::string>> g_label_info = {
  {"map_coco",
        {"label_map_coco.txt",
         "http://video.cambricon.com/models/labels/label_map_coco.txt"}
  },
  {"synset_word",
        {"synset_words.txt",
         "http://video.cambricon.com/models/labels/synset_words.txt"}
  }
};

std::string GetLabelInfoStr(std::string label_name, std::string info_type) {
  if (g_label_info.find(label_name) != g_label_info.end()) {
    if (info_type == "name") {
      return g_label_info.at(label_name).first;
    } else {
      return g_label_info.at(label_name).second;
    }
  }
  return "";
}
