/*************************************************************************
 * Copyright (C) [2020] by Cambricon, Inc. All rights reserved
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

#include "model.h"

#include <glog/logging.h>
#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/data_type.h"
#include "../../common/utils.hpp"

using std::string;
using std::vector;

namespace infer_server {

#define MM_SAFECALL(func, val)                                                    \
  do {                                                                            \
    auto ret = (func);                                                            \
    if (0 != ret) {                                                              \
      LOG(ERROR) << "[EasyDK InferServer] Call " #func " failed, ret = " << ret;  \
      return val;                                                                 \
    }                                                                             \
  } while (0)

bool ModelRunner::Init(AX_MModel* model, mm_unique_ptr<AX_Engine> engine, const std::vector<Shape>& input_shape) noexcept {
  input_num_ = model->GetInputNum();
  output_num_ = model->GetOutputNum();
  engine_ = std::move(engine);

  i_shapes_.resize(input_num_);
  o_shapes_.resize(output_num_);
  i_layouts_.resize(input_num_);
  o_layouts_.resize(output_num_);

  MM_SAFECALL(model->CreateInputTensors(&inputs_), false);
  MM_SAFECALL(model->CreateOutputTensors(&outputs_), false);

  auto dims2shape = [](const std::vector<int64_t>& d) { return Shape(d); };
  std::vector<std::vector<int64_t>> in_dims = model->GetInputDimensions();
  std::vector<Shape> in_shapes;
  std::transform(in_dims.begin(), in_dims.end(), std::back_inserter(in_shapes), dims2shape);

  return true;
}

ModelRunner::~ModelRunner() {
	
}

Status ModelRunner::Run(ModelIO* in, ModelIO* out) noexcept {  // NOLINT
  auto& input = in->surfs;
  auto& output = out->surfs;
  CHECK_EQ(input_num_, input.size()) << "EasyDK InferServer] [ModelRunner] Input number is mismatched";

  VLOG(5) << "[EasyDK InferServer] [ModelRunner] Process inference once, input num: " << input_num_ << " output num: "
          << output_num_;

  for (uint32_t i_idx = 0; i_idx < input_num_; ++i_idx) {
    inputs_[i_idx].SetData(input[i_idx]->GetData(0));
	
	/* to do, hlb  */
    if (!fixed_input_shape_) {
      LOG(ERROR) << "[EasyDK InferServer] [ModelRunner] here need to do";
    }
  }
  if (!output.empty()) {
    CHECK_EQ(output_num_, output.size()) << "[EasyDK InferServer] [ModelRunner] Output number is mismatched";
    for (uint32_t o_idx = 0; o_idx < output_num_; ++o_idx) {
      outputs_[o_idx].SetData(output[o_idx]->GetData(0));
    }
  }

  MM_SAFECALL(engine_->inference(), Status::ERROR_BACKEND);

  if (output.empty()) {
  	printf("here need to do\n");
  }

  return Status::SUCCESS;
}

bool Model::Init(const string& model_file, const std::vector<Shape>& in_shape) noexcept {
  model_file_ = model_file;
  model_.reset(CreateIModel());
  VLOG(1) << "[EasyDK InferServer] [Model] (success) Load model from graph file: " << model_file_;
  MM_SAFECALL(model_->init(model_file_), false);

  has_init_ = GetModelInfo(in_shape);
  return has_init_;
}

bool Model::Init(void* mem_ptr, size_t size, const std::vector<Shape>& in_shape) noexcept {
  std::ostringstream ss;
  ss << mem_ptr;
  model_file_ = ss.str();
  model_.reset(CreateIModel());

  VLOG(1) << "[EasyDK InferServer] [Model] (success) Load model from memory: " << model_file_;
  MM_SAFECALL(model_->init(mem_ptr, size), false);

  has_init_ = GetModelInfo(in_shape);
  return has_init_;
}

bool Model::GetModelInfo(const std::vector<Shape>& in_shape) noexcept {
  // get IO messages
  // get io number and data size
  i_num_ = model_->GetInputNum();
  o_num_ = model_->GetOutputNum();

  // get mlu io data type
  std::vector<axclrtEngineDataType> i_dtypes = model_->GetInputDataTypes();
  std::vector<axclrtEngineDataType> o_dtypes = model_->GetOutputDataTypes();

  std::vector<axclrtEngineDataLayout> i_dlayout = model_->GetInputDataLayouts();
  std::vector<axclrtEngineDataLayout> o_dlayout = model_->GetOutputDataLayouts();

  auto trans2layout = [](axclrtEngineDataType t, axclrtEngineDataLayout l) {
    DimOrder order = detail::CastDimOrder(l);
    if (order == DimOrder::INVALID) {
      LOG(WARNING) << "[EasyDK InferServer] [Model] DimOrder is invalid, use NHWC instead";
      order = DimOrder::NHWC;
    }
    return DataLayout{detail::CastDataType(t), order};
  };
  i_mlu_layouts_.reserve(i_num_);
  o_mlu_layouts_.reserve(o_num_);
  for (int idx = 0; idx < i_num_; ++idx) {
    i_mlu_layouts_.push_back(trans2layout(i_dtypes[idx], i_dlayout[idx]));
  }
  for (int idx = 0; idx < o_num_; ++idx) {
    o_mlu_layouts_.push_back(trans2layout(o_dtypes[idx], o_dlayout[idx]));
  }

  std::vector<std::vector<int64_t>> in_dims = model_->GetInputDimensions();
  std::vector<std::vector<int64_t>> out_dims = model_->GetOutputDimensions();

  auto dims2shape = [](const std::vector<int64_t>& d) { return Shape(d); };
  std::transform(in_dims.begin(), in_dims.end(), std::back_inserter(input_shapes_), dims2shape);
  std::transform(out_dims.begin(), out_dims.end(), std::back_inserter(output_shapes_), dims2shape);

  #if 0
  if (!FixedShape(input_shapes_)) {
    if (!in_shape.empty() && in_shape.size() == input_shapes_.size()) {
      VLOG(1) << "[EasyDK InferServer] [Model] Model with mutable input shape. Input shape is set by user.";
      for (unsigned idx = 0; idx < in_shape.size(); idx++) {
        input_shapes_[idx] = in_shape[idx];
        inputs[idx]->SetDimensions(mm::Dims(in_shape[idx].Vectorize()));
      }
      // MM_SAFECALL(ctx->InferOutputShape(inputs, outputs), {});
      auto ret = ctx->InferOutputShape(inputs, outputs);
      if (!ret.ok()) outputs.clear();
      for (uint32_t idx = 0; idx < outputs.size(); ++idx) {
        output_shapes_[idx] = outputs[idx]->GetDimensions().GetDims();
      }
    } else {
      VLOG(1) << "[EasyDK InferServer] [Model] Model with mutable input shape. Input shape is not set.";
    }
  }
  #endif

  switch (i_mlu_layouts_[0].order) {
    case DimOrder::NCHW:
    case DimOrder::NHWC:
    case DimOrder::HWCN:
      if (input_shapes_[0].Size() != 4) {
        LOG(ERROR) << "[EasyDK InferServer] [Model] Input shape and dim order is unmatched.";
        return false;
      }
      break;
    case DimOrder::NTC:
    case DimOrder::TNC:
      if (input_shapes_[0].Size() != 3) {
        LOG(ERROR) << "[EasyDK InferServer] [Model] Input shape and dim order is unmatched.";
        return false;
      }
      break;
    default:
      break;
  }
  switch (i_mlu_layouts_[0].order) {
    case DimOrder::NCHW:
    case DimOrder::NHWC:
    case DimOrder::NTC:
      model_batch_size_ = input_shapes_[0][0];
      break;
    case DimOrder::TNC:
      model_batch_size_ = input_shapes_[0][1];
      break;
    case DimOrder::HWCN:
      model_batch_size_ = input_shapes_[0][3];
      break;
    default:
      model_batch_size_ = input_shapes_[0][0];
      break;
  }

  VLOG(1) << "[EasyDK InferServer] [Model] Model Info: input number = " << i_num_ << ";\toutput number = " << o_num_;
  VLOG(1) << "[EasyDK InferServer] [Model]             batch size = " << model_batch_size_;
  for (int i = 0; i < i_num_; ++i) {
    VLOG(1) << "[EasyDK InferServer] [Model] ----- input index [" << i;
    VLOG(1) << "[EasyDK InferServer] [Model]       data type " << detail::DataTypeStr(i_mlu_layouts_[i].dtype);
    VLOG(1) << "[EasyDK InferServer] [Model]       dim order " << detail::DimOrderStr(i_mlu_layouts_[i].order);
    VLOG(1) << "[EasyDK InferServer] [Model]       shape " << input_shapes_[i];
  }
  for (int i = 0; i < o_num_; ++i) {
    VLOG(1) << "[EasyDK InferServer] [Model] ----- output index [" << i;
    VLOG(1) << "[EasyDK InferServer] [Model]       data type " << detail::DataTypeStr(o_mlu_layouts_[i].dtype);
    VLOG(1) << "[EasyDK InferServer] [Model]       dim order " << detail::DimOrderStr(o_mlu_layouts_[i].order);
    VLOG(1) << "[EasyDK InferServer] [Model]       shape " << output_shapes_[i];
  }
  return true;
}

Model::~Model() {
  VLOG(1) << "[EasyDK InferServer] [Model] Unload model: " << model_file_;
}
}  // namespace infer_server
