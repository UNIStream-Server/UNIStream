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

#ifndef INFER_SERVER_MM_HELPER_H_
#define INFER_SERVER_MM_HELPER_H_

#include <memory>
#ifdef HAVE_MM_COMMON_HEADER
#include "mm_common.h"
#include "mm_runtime.h"
#else
//#include "common.h"
//#include "interface_runtime.h"
#endif

namespace infer_server {

struct InferDeleter {
  template <typename T>
  void operator()(T *obj) const {
    /* to do */
    #if 0
    if (obj) {
      obj->Destroy();
    }
    #endif
  }
};
template <typename T>
using mm_unique_ptr = std::unique_ptr<T, InferDeleter>;

}  // namespace infer_server

#endif  // INFER_SERVER_MM_HELPER_H_
