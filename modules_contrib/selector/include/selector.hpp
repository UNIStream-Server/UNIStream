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

#ifndef MODULES_SELECTOR_HPP_
#define MODULES_SELECTOR_HPP_

#include <map>
#include <memory>
#include <string>

#include "unistream_frame.hpp"
#include "unistream_frame_va.hpp"
#include "unistream_module.hpp"
#include "private/unistream_param.hpp"
#include "strategy.hpp"

namespace unistream {

struct SelectorParams;
struct SelectorContext;

class Selector : public ModuleEx, public ModuleCreator<Selector> {
 public:
  explicit Selector(const std::string &name);
  virtual ~Selector();

  bool Open(ModuleParamSet param_set) override;
  void Close() override;
  int Process(UNIFrameInfoPtr data) override;

 private:
  void Select(UNIFrameInfoPtr current, UNIFrameInfoPtr provide, SelectorContext *ctx);
  SelectorContext *GetContext(UNIFrameInfoPtr data);

  std::unique_ptr<ModuleParamsHelper<SelectorParams>> param_helper_ = nullptr;
  std::map<std::string, SelectorContext *> contexts_;
  std::mutex mutex_;
};

}  // namespace unistream

#endif  // MODULES_SELECTOR_HPP_
