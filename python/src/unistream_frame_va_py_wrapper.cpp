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
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "pybind11/numpy.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "unistream_frame.hpp"
#include "unistream_frame_va.hpp"

#include "common_wrapper.hpp"

namespace py = pybind11;

namespace unistream {

extern std::shared_ptr<py::class_<UNIFrameInfo, std::shared_ptr<UNIFrameInfo>>> gPyframeRegister;

std::shared_ptr<UNIDataFrame> GetUNIDataFrame(std::shared_ptr<UNIFrameInfo> frame) {
  if (!frame->collection.HasValue(kUNIDataFrameTag)) {
    return nullptr;
  }
  return frame->collection.Get<std::shared_ptr<UNIDataFrame>>(kUNIDataFrameTag);
}

std::shared_ptr<UNIInferObjs> GetUNIInferObjects(std::shared_ptr<UNIFrameInfo> frame) {
  if (!frame->collection.HasValue(kUNIInferObjsTag)) {
    return nullptr;
  }
  return frame->collection.Get<std::shared_ptr<UNIInferObjs>>(kUNIInferObjsTag);
}

void UNIDataFrameWrapper(const py::module &m) {
  py::class_<UNIDataFrame, std::shared_ptr<UNIDataFrame>>(m, "UNIDataFrame")
      .def(py::init([]() {
        return std::make_shared<UNIDataFrame>();
      }))
      .def("image_bgr", [](std::shared_ptr<UNIDataFrame> data_frame) {
        cv::Mat bgr_img = data_frame->ImageBGR();
        return MatToArray(bgr_img);
      })
      .def("has_bgr_image", &UNIDataFrame::HasBGRImage)
      .def_readwrite("buf_surf", &UNIDataFrame::buf_surf)
      .def_readwrite("frame_id", &UNIDataFrame::frame_id);
}

void UNIInferObjsWrapper(const py::module &m) {
  py::class_<UNIInferObjs, std::shared_ptr<UNIInferObjs>>(m, "UNIInferObjs")
      .def(py::init([]() {
        return std::make_shared<UNIInferObjs>();
      }))
      .def_property("objs", [](std::shared_ptr<UNIInferObjs> objs_holder) {
          std::lock_guard<std::mutex> lk(objs_holder->mutex_);
          return objs_holder->objs_;
      }, [](std::shared_ptr<UNIInferObjs> objs_holder, std::vector<std::shared_ptr<UNIInferObject>> objs) {
          std::lock_guard<std::mutex> lk(objs_holder->mutex_);
          objs_holder->objs_ = objs;
      })
      .def("push_back", [](std::shared_ptr<UNIInferObjs> objs_holder, std::shared_ptr<UNIInferObject> obj) {
          std::lock_guard<std::mutex> lk(objs_holder->mutex_);
          objs_holder->objs_.push_back(obj);
      });


  py::class_<UNIInferAttr, std::shared_ptr<UNIInferAttr>>(m, "UNIInferAttr")
      .def(py::init([]() {
        return std::make_shared<UNIInferAttr>();
      }))
      .def(py::init([](int id, int value, float score) {
        auto attr = std::make_shared<UNIInferAttr>();
        attr->id = id;
        attr->value = value;
        attr->score = score;
        return attr;
      }))
      .def_readwrite("id", &UNIInferAttr::id)
      .def_readwrite("value", &UNIInferAttr::value)
      .def_readwrite("score", &UNIInferAttr::score);


  py::class_<UNIInferObject, std::shared_ptr<UNIInferObject>>(m, "UNIInferObject")
      .def(py::init([]() {
        return std::make_shared<UNIInferObject>();
      }))
      .def_readwrite("id", &UNIInferObject::id)
      .def_readwrite("track_id", &UNIInferObject::track_id)
      .def_readwrite("score", &UNIInferObject::score)
      .def_readwrite("bbox", &UNIInferObject::bbox)
      .def("get_py_collection", [](std::shared_ptr<UNIInferObject> obj) {
          if (!obj->collection.HasValue("py_collection")) {
            obj->collection.Add("py_collection", py::dict());
          }
          return obj->collection.Get<py::dict>("py_collection");
      })
      .def("add_attribute", [](std::shared_ptr<UNIInferObject> obj, const std::string& key, const UNIInferAttr& attr) {
        obj->AddAttribute(key, attr);
      })
      .def("get_attribute", &UNIInferObject::GetAttribute)
      .def("add_extra_attribute", &UNIInferObject::AddExtraAttribute)
      .def("add_extra_attributes", &UNIInferObject::AddExtraAttributes)
      .def("get_extra_attribute", &UNIInferObject::GetExtraAttribute)
      .def("remove_extra_attribute", &UNIInferObject::RemoveExtraAttribute)
      .def("get_extra_attributes", &UNIInferObject::GetExtraAttributes)
      .def("add_feature", &UNIInferObject::AddFeature)
      .def("get_feature", &UNIInferObject::GetFeature)
      .def("get_features", &UNIInferObject::GetFeatures);
}

void UNIFrameVaWrapper(const py::module &m) {
  UNIDataFrameWrapper(m);
  UNIInferObjsWrapper(m);

  gPyframeRegister->def("get_cn_data_frame", &GetUNIDataFrame);
  gPyframeRegister->def("get_cn_infer_objects", &GetUNIInferObjects);
}

}  //  namespace unistream
