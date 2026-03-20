/*************************************************************************
 * Copyright (C) [2022] by Cambricon, Inc. All rights reserved
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

#include <map>
#include <memory>

#include "encode_handler_mlu.hpp"

#include "uniedk_platform.h"
#include "unistream_frame_va.hpp"
#include "platform_utils.hpp"

namespace unistream {

static UniedkBufSurfaceColorFormat GetSufaceColorFromBuffer(Scaler::ColorFormat format) {
  static std::map<Scaler::ColorFormat, UniedkBufSurfaceColorFormat> color_map {
    {Scaler::ColorFormat::YUV_I420,  UNIEDK_BUF_COLOR_FORMAT_YUV420},
    {Scaler::ColorFormat::YUV_NV12,  UNIEDK_BUF_COLOR_FORMAT_NV12},
    {Scaler::ColorFormat::YUV_NV21,  UNIEDK_BUF_COLOR_FORMAT_NV21},
    {Scaler::ColorFormat::BGR,       UNIEDK_BUF_COLOR_FORMAT_BGR},
    {Scaler::ColorFormat::RGB,       UNIEDK_BUF_COLOR_FORMAT_RGB},
    {Scaler::ColorFormat::BGRA,      UNIEDK_BUF_COLOR_FORMAT_BGRA},
    {Scaler::ColorFormat::RGBA,      UNIEDK_BUF_COLOR_FORMAT_RGBA},
    {Scaler::ColorFormat::ABGR,      UNIEDK_BUF_COLOR_FORMAT_ABGR},
    {Scaler::ColorFormat::ARGB,      UNIEDK_BUF_COLOR_FORMAT_ARGB},
  };

  if (color_map.find(format) != color_map.end()) return color_map[format];
  return UNIEDK_BUF_COLOR_FORMAT_LAST;
}

VencMluHandler::VencMluHandler(int dev_id) { dev_id_ = dev_id; }
VencMluHandler::~VencMluHandler() {
  std::unique_lock<std::mutex> guard(mutex_);
  if (!eos_promise_) {
    eos_promise_.reset(new std::promise<void>);
    if (UniedkVencSendFrame(venc_handle_, nullptr, 2000) < 0) {  // send eos
      LOGE(VENC) << "UniedkVencSend Eos error";
      eos_promise_.reset(nullptr);
    }
  }
  if (venc_handle_) {
    UniedkVencDestroy(venc_handle_);
    venc_handle_ = nullptr;
  }
  if (eos_promise_) {
    eos_promise_->get_future().wait();
    eos_promise_.reset(nullptr);
  }
}

int VencMluHandler::SendFrame(Scaler::Buffer* buffer) {
  if (!buffer) {
    eos_promise_.reset(new std::promise<void>);
    if (UniedkVencSendFrame(venc_handle_, nullptr, 2000) < 0) {  // send eos
      LOGE(VENC) << "UniedkVencSend Eos error";
      eos_promise_.reset(nullptr);
      return -1;
    }
    return 0;
  }

  axclrtDeviceList deviceList;
  (void)axclrtGetDeviceList(&deviceList);
  axclrtSetDevice(deviceList.devices[dev_id_]);

  std::unique_lock<std::mutex> guard(mutex_);
  if (!venc_handle_) {
    uint32_t width = buffer->width;
    uint32_t height = buffer->height;
    UniedkBufSurfaceColorFormat color_format;
    color_format = GetSufaceColorFromBuffer(buffer->color);
    if (color_format == UNIEDK_BUF_COLOR_FORMAT_LAST) {
      LOGE(VENC) << "not support this corlor format";
      return -1;
    }
    InitEncode(width, height, color_format);

    UniedkPlatformInfo platform;
    UniedkPlatformGetInfo(dev_id_, &platform);
    platform_ = platform.name;
  }
  guard.unlock();

  if (IsCloudPlatform(platform_)) {
    UniedkBufSurface surface;
    UniedkBufSurfaceParams params;
    memset(&surface, 0, sizeof(UniedkBufSurface));
    memset(&params, 0, sizeof(UniedkBufSurfaceParams));

    params.width = buffer->width;
    params.height = buffer->height;
    params.pitch = buffer->stride[0];
    params.color_format = GetSufaceColorFromBuffer(buffer->color);
    params.data_ptr = reinterpret_cast<void *>(buffer->data[0]);

    if (buffer->color == Scaler::ColorFormat::YUV_NV12 ||
        buffer->color == Scaler::ColorFormat::YUV_NV21) {
      params.data_size = buffer->stride[0] * buffer->height * 3 / 2;
      params.plane_params.num_planes = 2;
      params.plane_params.width[0] = buffer->width;
      params.plane_params.width[1] = buffer->width;
      params.plane_params.height[0] = buffer->height;
      params.plane_params.height[1] = buffer->height;
      params.plane_params.pitch[0] = buffer->stride[0];
      params.plane_params.pitch[1] = buffer->stride[1];
      params.plane_params.offset[0] = 0;
      params.plane_params.offset[1] = buffer->stride[0] * buffer->height;
    } else {
      LOGE(VENC) << "Not support color format: " << buffer->color;
      return -1;
    }
    /*else if (buffer->color == Scaler::ColorFormat::YUV_I420) {
      params.data_size = buffer->stride[0] * buffer->height * 3 / 2;
      params.plane_params.num_planes = 3;
      params.plane_params.width[0] = buffer->width;
      params.plane_params.width[1] = buffer->width;
      params.plane_params.width[2] = buffer->width;
      params.plane_params.height[0] = buffer->height;
      params.plane_params.height[1] = buffer->height;
      params.plane_params.height[2] = buffer->height;
      params.plane_params.pitch[0] = buffer->stride[0];
      params.plane_params.pitch[1] = buffer->stride[1];
      params.plane_params.pitch[2] = buffer->stride[2];
      params.plane_params.offset[0] = 0;
      params.plane_params.offset[1] = buffer->stride[0] * buffer->height;
      params.plane_params.offset[2] = buffer->stride[0] * buffer->height * 5 * 4;
    } else if (buffer->color == Scaler::ColorFormat::BGR ||
               buffer->color == Scaler::ColorFormat::RGB) {
      params.data_size = buffer->stride[0] * buffer->height * 3;
      params.plane_params.num_planes = 1;
      params.plane_params.width[0] = buffer->width;
      params.plane_params.height[0] = buffer->height;
      params.plane_params.pitch[0] = buffer->stride[0];
    } else if (buffer->color == Scaler::ColorFormat::ABGR ||
               buffer->color == Scaler::ColorFormat::ARGB ||
               buffer->color == Scaler::ColorFormat::BGRA ||
               buffer->color == Scaler::ColorFormat::RGBA) {
      params.data_size = buffer->stride[0] * buffer->height * 4;
      params.plane_params.num_planes = 1;
      params.plane_params.width[0] = buffer->width;
      params.plane_params.height[0] = buffer->height;
      params.plane_params.pitch[0] = buffer->stride[0];
    }
    */

    surface.surface_list = &params;
    surface.mem_type = UNIEDK_BUF_MEM_SYSTEM;
    surface.batch_size = 1;
    surface.num_filled = 1;
    surface.pts = 0;

    if (UniedkVencSendFrame(venc_handle_, &surface, 2000) < 0) {
      LOGE(VENC) << "UniedkVencSendFrame failed";
      return -1;
    }
  }

  return 0;
}

int VencMluHandler::InitEncode(int width, int height, UniedkBufSurfaceColorFormat color_format) {
  UniedkVencCreateParams params;
  memset(&params, 0, sizeof(params));
  switch (param_.codec_type) {
    case VideoCodecType::H264:
      params.type = UNIEDK_VENC_TYPE_H264;
      break;
    case VideoCodecType::H265:
      params.type = UNIEDK_VENC_TYPE_H265;
      break;
    case VideoCodecType::JPEG:
      params.type = UNIEDK_VENC_TYPE_JPEG;
      break;
    default:
      params.type = UNIEDK_VENC_TYPE_H264;
      break;
  }

  params.device_id = dev_id_;
  params.width = width;
  params.height = height;

  params.color_format = color_format;
  params.frame_rate = param_.frame_rate;
  params.key_interval = 0;  // not used by CE3226
  params.input_buf_num = 3;  // not used by CE3226
  params.gop_size = param_.gop_size;
  params.bitrate = param_.bitrate;
  params.OnFrameBits = VencMluHandler::OnFrameBits_;
  params.OnEos = VencMluHandler::OnEos_;
  params.OnError = VencMluHandler::OnError_;
  params.userdata = this;
  int ret = UniedkVencCreate(&venc_handle_, &params);
  if (ret < 0) {
    LOGE(VENC) << "UniedkVencCreate failed";
    return -1;
  }
  return 0;
}

int VencMluHandler::SendFrame(std::shared_ptr<UNIFrameInfo> data) {
  if (!data || data->IsEos()) {
    eos_promise_.reset(new std::promise<void>);
    if (UniedkVencSendFrame(venc_handle_, nullptr, 2000) < 0) {  // send eos
      LOGE(VENC) << "UniedkVencSendFrame Eos error";
      eos_promise_.reset(nullptr);
      return -1;
    }
    return 0;
  }

  axclrtDeviceList deviceList;
  (void)axclrtGetDeviceList(&deviceList);
  axclrtSetDevice(deviceList.devices[dev_id_]);

  UNIDataFramePtr frame = data->collection.Get<UNIDataFramePtr>(kUNIDataFrameTag);

  if (!frame->buf_surf) {
    LOGE(VENC) << "surface is nullptr";
    return -1;
  }
  std::unique_lock<std::mutex> guard(mutex_);
  if (!venc_handle_) {
    uint32_t width = param_.width;
    uint32_t height = param_.height;
    if (width == 0) {
      width = frame->buf_surf->GetWidth();
    }
    if (height == 0) {
      height = frame->buf_surf->GetHeight();
    }
    InitEncode(width, height, frame->buf_surf->GetColorFormat());

    UniedkPlatformInfo platform;
    UniedkPlatformGetInfo(dev_id_, &platform);
    platform_ = platform.name;
  }

  guard.unlock();

  if (UniedkVencSendFrame(venc_handle_, frame->buf_surf->GetBufSurface(), 2000) < 0) {
    LOGE(VENC) << "UniedkVencSendFrame failed";
    return -1;
  }

  return 0;
}

int VencMluHandler::OnEos() {
  if (eos_promise_) {
    eos_promise_->set_value();
  }
  LOGI(VENC) << "VEncode::OnEos() called";
  return 0;
}

int VencMluHandler::OnError(int errcode) {
  LOGE(VENC) << "VEncode::OnError() called, FIXME" << std::hex << errcode << std::dec;
  return 0;
}

}  // namespace unistream
