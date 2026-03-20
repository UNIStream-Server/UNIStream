/*************************************************************************
 * Copyright (C) [2025] by UNIStream Team. All rights reserved 
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

#ifndef UNIEDK_MODEL_HPP_
#define UNIEDK_MODEL_HPP_

#include <string>
#include <memory>
#include <vector>
#include <axcl.h>

class AX_Engine{
 public:
  AX_Engine() = default;
  ~AX_Engine();
  int inference() noexcept;

 public:
  uint64_t modelId = 0;
  axclrtEngineIOInfo io_info = 0;
  uint64_t context = 0;
  axclrtEngineIO io_ = 0;
};  // class AX_Engine

class AX_Tensor{
 public:
  AX_Tensor() = default;
  ~AX_Tensor(){
  	fprintf(stderr, "[EasyDK InferServer] [~AX_Tensor] destroy AX_Tensor\n");
  }
  int SetData(void *) noexcept;
  
 public:
  uint64_t size_ = 0;
  bool bInputData{true};
  axclrtEngineIO io_ = 0;
  uint32_t index_ = 0;
};  // class Model


class AX_MModel{
 public:
  AX_MModel() = default;
  ~AX_MModel();
  int init(const std::string& model_file) noexcept;
  int init(const void* mem_ptr, const size_t size) noexcept;
  int GetInputNum(void) noexcept;
  int GetOutputNum(void) noexcept;
  std::vector<axclrtEngineDataType> GetInputDataTypes(void) noexcept;
  std::vector<axclrtEngineDataType> GetOutputDataTypes(void) noexcept;
  std::vector<axclrtEngineDataLayout> GetInputDataLayouts(void) noexcept;
  std::vector<axclrtEngineDataLayout> GetOutputDataLayouts(void) noexcept;
  std::vector<std::vector<int64_t>> GetInputDimensions(void) noexcept;
  std::vector<std::vector<int64_t>> GetOutputDimensions(void) noexcept;
  AX_Engine* CreateEngine(void) noexcept;
  int CreateInputTensors(std::vector<AX_Tensor>* inputs) noexcept;
  int CreateOutputTensors(std::vector<AX_Tensor>* outputs) noexcept;

 private:
  uint64_t modelId = 0;
  uint64_t context = 0;
  axclrtEngineIOInfo io_info;
  uint32_t inputNum = 0;
  uint32_t outputNum = 0;
  int group_count = 0;
  std::vector<axclrtEngineIO> ios;

  std::vector<axclrtEngineDataType> input_types;
  std::vector<axclrtEngineDataLayout> input_layouts;
  std::vector<std::vector<int64_t>> input_dims;
  std::vector<std::vector<uint64_t>> input_sizes;
  
  std::vector<axclrtEngineDataType> output_types;
  std::vector<axclrtEngineDataLayout> output_layouts;
  std::vector<std::vector<int64_t>> output_dims;
  std::vector<std::vector<uint64_t>> output_sizes;

  AX_Engine *engine{nullptr};
};  // class Model

AX_MModel *CreateIModel();

#endif  // UNIEDK_MODEL_HPP_