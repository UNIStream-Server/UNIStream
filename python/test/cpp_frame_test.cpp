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
#include <memory>

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "unistream_frame.hpp"

namespace py = pybind11;

namespace unistream {

class CppUNIFrameInfoTestHelper {
 public:
  CppUNIFrameInfoTestHelper() {
    frame_info_ = unistream::UNIFrameInfo::Create("test_stream_id_0");
  }
  std::shared_ptr<UNIFrameInfo> GetFrameInfo() {
    return frame_info_;
  }
 private:
  std::shared_ptr<UNIFrameInfo> frame_info_;
};  //  class CppUNIFrameInfoTestHelper

}  // namespace unistream

void FrameTestWrapper(const py::module& m) {
  py::class_<unistream::CppUNIFrameInfoTestHelper>(m, "CppUNIFrameInfoTestHelper")
    .def(py::init())
    .def("get_frame_info", &unistream::CppUNIFrameInfoTestHelper::GetFrameInfo);
}

