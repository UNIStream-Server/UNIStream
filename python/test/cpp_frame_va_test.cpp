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
#include "unistream_frame_va.hpp"

namespace py = pybind11;

namespace unistream {

void SetDataFrame(std::shared_ptr<UNIFrameInfo> frame, std::shared_ptr<UNIDataFrame> dataframe) {
  // TODO(gaoyujia): create buf
  frame->collection.Add(kUNIDataFrameTag, dataframe);
}

void SetUNIInferobjs(std::shared_ptr<UNIFrameInfo> frame, std::shared_ptr<UNIInferObjs> objs_holder) {
  frame->collection.Add(kUNIInferObjsTag, objs_holder);
}

}  // namespace unistream

void FrameVaTestWrapper(py::module& m) {  // NOLINT
  m.def("set_data_frame", &unistream::SetDataFrame);
  m.def("set_infer_objs", &unistream::SetUNIInferobjs);
}

