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

#ifndef INFER_SERVER_PROCESSOR_H_
#define INFER_SERVER_PROCESSOR_H_

#include <memory>
#include <string>
#include <vector>

#include "uniedk_transform.h"
#include "uniedk_buf_surface_util.hpp"
#include "infer_server.h"
#include "shape.h"

namespace infer_server {

// -------------------- Predictor --------------------
/**
 * @brief a structure storing input and output data of inference
 */
struct ModelIO {
  /// input / output data
  std::vector<uniedk::BufSurfWrapperPtr> surfs;
  /// shape of input / output
  std::vector<Shape> shapes;
};

struct PredictorPrivate;
/**
 * @brief Predictor processor
 */
class Predictor : public ProcessorForkable<Predictor> {
 public:
  /**
   * @brief Construct a new Predictor object
   */
  Predictor() noexcept;

  /**
   * @brief Destroy the Predictor object
   */
  ~Predictor();

  /**
   * @brief Perform predict
   *
   * @param data processed data
   * @retval Status::SUCCESS Succeeded
   * @retval Status::INVALID_PARAM Predictor get discontinuous data
   * @retval Status::WRONG_TYPE Predictor get data of wrong type (bad_any_cast)
   * @retval Status::ERROR_BACKEND Predict failed
   */
  Status Process(PackagePtr data) noexcept override;

  /**
   * @brief Initialize predictor
   *
   * @retval Status::SUCCESS Init succeeded
   * @retval Status::INVALID_PARAM Predictor donot have enough params or get wrong params, @see BaseObject::SetParam
   * @retval Status::WRONG_TYPE Predictor get params of wrong type (bad_any_cast)
   */
  Status Init() noexcept override;

  /**
   * @brief Get backend string
   *
   * @retval magicmind Use magicmind to do inference
   * @retval Use model to do inference
   */
  static std::string Backend() noexcept;

 private:
  PredictorPrivate* priv_;
};  // class Predictor
// -------------------- Predictor END --------------------

// -------------------- Preprocessor  --------------------
struct UniPreprocTensorParams {
  DimOrder input_order;
  std::vector<int> input_shape;  // interpreted based on the input order
  NetworkInputFormat input_format;  // model_input_format
  DataType input_dtype;
  uint32_t batch_num;
};

class IPreproc {
 public:
  virtual ~IPreproc() {}
  virtual int OnTensorParams(const UniPreprocTensorParams *params) = 0;
  virtual int OnPreproc(uniedk::BufSurfWrapperPtr src, uniedk::BufSurfWrapperPtr dst,
                        const std::vector<UniedkTransformRect> &src_rects) = 0;
};

void SetPreprocHandler(const std::string &key, IPreproc *handler);
IPreproc *GetPreprocHandler(const std::string &key);
void RemovePreprocHandler(const std::string &key);

//
struct UNIInferBoundingBox {
  float x = 0.0;
  float y = 0.0;
  float w = 0.0;
  float h = 0.0;
  explicit UNIInferBoundingBox(float x = 0.0, float y = 0.0, float w = 0.0, float h = 0.0) : x(x), y(y), w(w), h(h) {}
};

struct PreprocInput {
  uniedk::BufSurfWrapperPtr surf = nullptr;
  bool has_bbox = false;
  UNIInferBoundingBox bbox;
};

class PreprocImpl;
// assume that inference model has only one input
class Preprocessor : public ProcessorForkable<Preprocessor> {
 public:
  Preprocessor() noexcept;
  ~Preprocessor();

  Status Init() noexcept override;
  Status Process(PackagePtr data) noexcept override;

 private:
  PreprocImpl *impl_ = nullptr;
};

// -------------------- Preprocessor END --------------------

// -------------------- Postprocessor --------------------
class IPostproc {
 public:
  virtual ~IPostproc() {}
  virtual int OnPostproc(const std::vector<InferData*>& data_vec, const infer_server::ModelIO& output,
                         const infer_server::ModelInfo* info) = 0;
};

void SetPostprocHandler(const std::string &key, IPostproc *handler);
IPostproc *GetPostprocHandler(const std::string &key);
void RemovePostprocHandler(const std::string &key);

struct PostprocessorPrivate;
/**
 * @brief Postprocessor processor
 */
class Postprocessor : public ProcessorForkable<Postprocessor> {
 public:
  /**
   * @brief Construct a new Postprocessor object
   */
  Postprocessor() noexcept;

  /**
   * @brief Destroy the Postprocessor object
   */
  ~Postprocessor();

  /**
   * @brief Perform postprocess
   *
   * @param data processed data
   * @retval Status::SUCCESS Succeeded
   * @retval Status::INVALID_PARAM Postprocessor get discontinuous data
   * @retval Status::WRONG_TYPE Postprocessor get data of wrong type (bad_any_cast)
   * @retval Status::ERROR_BACKEND Postprocess failed in setting current device or postprocessing
   */
  Status Process(PackagePtr data) noexcept override;

  /**
   * @brief Initialize PostprocessorHost
   *
   * @retval Status::SUCCESS Init succeeded
   * @retval Status::INVALID_PARAM Postprocessor do not have enough params or get wrong params,
   *         @see BaseObject::SetParam
   * @retval Status::WRONG_TYPE Postprocessor get params of wrong type (bad_any_cast)
   */
  Status Init() noexcept override;

 private:
  PostprocessorPrivate* priv_;
};  // class Postprocessor
// -------------------- Postprocessor END --------------------

}  // namespace infer_server

#endif  // INFER_SERVER_PROCESSOR_H_
