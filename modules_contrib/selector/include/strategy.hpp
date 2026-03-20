/*************************************************************************
 * Copyright (C) [2020] by Cambricon, Inc. All rights reserved
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

#ifndef MODULES_SELECTOR_STRATEGY_HPP_
#define MODULES_SELECTOR_STRATEGY_HPP_

#include <memory>
#include <string>
#include <vector>

#include "unistream_frame_va.hpp"
#include "unistream_logging.hpp"
#include "reflex_object.h"

namespace unistream {

using UNIInferObjectPtr = std::shared_ptr<UNIInferObject>;

class Strategy : virtual public ReflexObjectEx<Strategy> {
 public:
  static Strategy *Create(const std::string &name);
  virtual ~Strategy() {}

  virtual bool Process(const UNIInferObjectPtr &obj, int64_t frame_id) = 0;
  virtual bool Check(const UNIInferObjectPtr &obj, int64_t frame_id) { return true; }
  virtual void UpdateFrame() {}
  virtual bool Config(const std::string &params) { return true; }
};

}  // namespace unistream

#endif  // MODULES_SELECTOR_STRATEGY_HPP_
