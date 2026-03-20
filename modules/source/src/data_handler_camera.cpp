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
#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#include "uniedk_buf_surface_util.hpp"
#include "uniedk_platform.h"
#include "unistream_logging.hpp"
#include "data_handler_camera.hpp"
#include "data_handler_util.hpp"
#include "data_source.hpp"
#include "profiler/module_profiler.hpp"
#include "profiler/pipeline_profiler.hpp"
#include "video_capture.hpp"

namespace unistream {

class CameraHandlerImpl : public SourceRender, public ICaptureResult, public IUserPool {
 public:
  explicit CameraHandlerImpl(DataSource *module, const SensorSourceParam &param, CameraHandler *handler)
      : SourceRender(handler),
        module_(module),
        handle_param_(param),
        handler_(*handler),
        stream_id_(handler_.GetStreamId()) {}
  ~CameraHandlerImpl() { Close(); }
  bool Open();
  void Stop();
  void Close();

 private:
  DataSource *module_ = nullptr;
  SensorSourceParam handle_param_;
  CameraHandler &handler_;
  std::string stream_id_;
  DataSourceParam param_;

 private:
  void Loop();
  void OnCaptureEos();

  // ICaptureResult
  void OnCaptureFrame(uniedk::BufSurfWrapperPtr buf_surf) override;
  void OnCaptureError(int err_code) override;

  // IUserPool
  int CreatePool(UniedkBufSurfaceCreateParams *params, uint32_t block_count) override {
    return pool_.CreatePool(params, block_count);
  }
  void DestroyPool() override { pool_.DestroyPool(5000); }
  void OnBufInfo(int width, int height, UniedkBufSurfaceColorFormat fmt) {
    // TODO(liujian): we create the pool advanced (ignore Buffer Info) at the moment.
    return;
  }
  uniedk::BufSurfWrapperPtr GetBufSurface(int timeout_ms) override { return pool_.GetBufSurfaceWrapper(timeout_ms); }
  /**/
  std::atomic<int> running_{0};
  std::thread thread_;
  bool eos_sent_ = false;

 private:
  uniedk::BufPool pool_;
  bool eos_reached_ = false;
  std::shared_ptr<IVinCapture> capture_ = nullptr;
};  // class FileHandlerImpl

std::shared_ptr<SourceHandler> CreateSource(DataSource *module, const std::string &stream_id,
                                            const SensorSourceParam &param) {
  if (!module || stream_id.empty()) {
    LOGE(SOURCE) << "CreateSource(): Create CameraHandler failed. source module and stream id must not be empty";
    return nullptr;
  }
  return std::make_shared<CameraHandler>(module, stream_id, param);
}

CameraHandler::CameraHandler(DataSource *module, const std::string &stream_id, const SensorSourceParam &param)
    : SourceHandler(module, stream_id) {
  impl_ = new (std::nothrow) CameraHandlerImpl(module, param, this);
}

CameraHandler::~CameraHandler() {
  if (impl_) delete impl_, impl_ = nullptr;
}

bool CameraHandler::Open() {
  if (!this->module_) {
    LOGE(SOURCE) << "[CameraHandler] Open(): [" << stream_id_ << "]: module_ null";
    return false;
  }
  if (!impl_) {
    LOGE(SOURCE) << "[CameraHandler] Open(): [" << stream_id_ << "]: no memory left";
    return false;
  }

  if (stream_index_ == unistream::kInvalidStreamIdx) {
    LOGE(SOURCE) << "[CameraHandler] Open(): [" << stream_id_ << "]: Invalid stream_idx";
    return false;
  }

  return impl_->Open();
}

void CameraHandler::Stop() {
  if (impl_) {
    impl_->Stop();
  }
}
void CameraHandler::Close() {
  if (impl_) {
    impl_->Close();
  }
}

bool CameraHandlerImpl::Open() {
  DataSource *source = dynamic_cast<DataSource *>(module_);
  param_ = source->GetSourceParam();

  // TODO(liujian): to support alloc pool according to the picture size
  if (handle_param_.out_res.width <= 0) handle_param_.out_res.width = 1920;
  if (handle_param_.out_res.height <= 0) handle_param_.out_res.height = 1080;

  LOGI(SOURCE) << "[CameraHandlerImpl] Open(): w = " << handle_param_.out_res.width
               << ", h = " << handle_param_.out_res.height;
  UniedkBufSurfaceCreateParams create_params;
  memset(&create_params, 0, sizeof(create_params));
  create_params.device_id = param_.device_id;
  create_params.batch_size = 1;
  create_params.color_format = UNIEDK_BUF_COLOR_FORMAT_NV12;
  create_params.width = handle_param_.out_res.width;
  create_params.height = handle_param_.out_res.height;
  create_params.mem_type = UNIEDK_BUF_MEM_VB_CACHED;
  if (CreatePool(&create_params, param_.bufpool_size) < 0) {
      LOGE(SOURCE) << "[CameraHandlerImpl] Open(): Create pool failed";
    return false;
  }

  // start separate thread
  running_.store(1);
  thread_ = std::thread(&CameraHandlerImpl::Loop, this);
  return true;
}

void CameraHandlerImpl::Stop() {
  if (running_.load()) {
    running_.store(0);
    if (thread_.joinable()) {
      thread_.join();
    }
  }
}

void CameraHandlerImpl::Close() {
  Stop();
  LOGI(SOURCE) << "[CameraHandlerImpl] Close(): this(" << this << ") Destroy pool";
  DestroyPool();
}

void CameraHandlerImpl::Loop() {
  VLOG1(SOURCE) << "[CameraHandlerImpl] Loop(): [" << stream_id_ << "]: loop";

  UniedkCreateThdContext(param_.device_id);
  capture_ = std::make_shared<VinCapture>(stream_id_, this, this);
  if (!capture_)
  {
    (void)UniedkDestroyThdContext();
    return;
  }
  capture_->Create(handle_param_.sensor_id);

  int timeout_ms = 1000;
  while (running_.load()) {
    if (capture_->Process(timeout_ms) == false) {
      break;
    }
  }
  // before exit, send eos
  OnCaptureEos();
  VLOG1(SOURCE) << "[CameraHandlerImpl] Loop(): [" << stream_id_ << "]: loop exit.";
  (void)UniedkDestroyThdContext();
}

void CameraHandlerImpl::OnCaptureFrame(uniedk::BufSurfWrapperPtr wrapper) {
  std::shared_ptr<UNIFrameInfo> data = this->CreateFrameInfo();
  if (!data) {
    LOGW(SOURCE) << "[CameraHandlerImpl] OnCaptureFrame(): failed to create FrameInfo.";
    return;
  }
  data->timestamp = wrapper->GetPts();  // FIXME
  int ret = SourceRender::Process(data, std::move(wrapper), frame_id_++, param_);
  if (ret < 0) {
    LOGE(SOURCE) << "[CameraHandlerImpl] OnCaptureFrame(): [" << stream_id_ << "]: Render frame failed";
    return;
  }
  this->SendFrameInfo(data);
}

void CameraHandlerImpl::OnCaptureError(int err_code) {
  this->SendFlowEos();
  LOGI(SOURCE) << "[CameraHandlerImpl] OnCaptureError(): called";
}

void CameraHandlerImpl::OnCaptureEos() {
  this->SendFlowEos();
  LOGI(SOURCE) << "[CameraHandlerImpl] OnCaptureEos(): called";
}

}  // namespace unistream
