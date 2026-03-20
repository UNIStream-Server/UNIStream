/*************************************************************************
 * Copyright (C) [2021] by Cambricon, Inc. All rights reserved
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
#include "vout.hpp"

#include <string>
#include <memory>
#include <vector>

#include "uniedk_vout_display.h"
#include "unistream_logging.hpp"

namespace unistream {

Vout::Vout(const std::string &name) : ModuleEx(name) {
  param_register_.SetModuleDesc("Vout is a module to render frames.");
  param_helper_.reset(new (std::nothrow) ModuleParamsHelper<VoutParam>(name));
  static const std::vector<ModuleParamDesc> regist_param = {
      {"stream_id", "", "Which stream will be rendered.", PARAM_OPTIONAL, OFFSET(VoutParam, stream_id),
        ModuleParamParser<std::string>::Parser, "string"}
  };
  param_helper_->Register(regist_param, &param_register_);
}

Vout::~Vout() {}

bool Vout::Open(ModuleParamSet param_set) {
  if (!CheckParamSet(param_set)) {
    return false;
  }
  param_.framerate = 30;
  return true;
}

void Vout::Close() {
  // render_uninit();
}

int Vout::Process(std::shared_ptr<UNIFrameInfo> data) {
  if (!data) {
    LOGE(VOUT) << "Process input data is nulltpr!";
    return -1;
  }

  if (data->IsRemoved()) {
    return 0;
  }

  auto params = param_helper_->GetParams();

  if (!data->IsEos()) {
    // TODO(liujian)
    //   generate 4 channels output, and render ...
    //
    UNIDataFramePtr frame = data->collection.Get<UNIDataFramePtr>(kUNIDataFrameTag);
    if (params.stream_id.empty()) {
      // render channel 0 by default
      if (data->GetStreamIndex() == 0) {
        //UniedkVoutRender(frame->buf_surf->GetBufSurface());
      }
    } else {
      if (data->stream_id == params.stream_id) {
        //UniedkVoutRender(frame->buf_surf->GetBufSurface());
      }
    }
  } else {
    // EOS
    // ...
  }
  TransmitData(data);  // forward by this module
  return 0;
}

bool Vout::CheckParamSet(const ModuleParamSet &param_set) const {
  if (!param_helper_->ParseParams(param_set)) {
    LOGE(VOUT) << "[" << GetName() << "] parse parameters failed.";
    return false;
  }
  return true;  // FIXME
}

}  // namespace unistream
