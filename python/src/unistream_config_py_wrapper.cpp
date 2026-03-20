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

#include <string>

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "unistream_config.hpp"

namespace py = pybind11;

namespace unistream {

namespace detail {

class Pybind11ConfigV : public UNIConfigBase {
 public:
  bool ParseByJSONStr(const std::string &jstr) override {
    PYBIND11_OVERRIDE_PURE(bool, UNIConfigBase, ParseByJSONStr, jstr);
  }
};  // class Pybind11ConfigV

}  // namespace detail

void ConfigWrapper(py::module &m) {  // NOLINT
  py::class_<UNIConfigBase, detail::Pybind11ConfigV>(m, "UNIConfigBase")
      .def(py::init())
      .def("parse_by_json_file", &UNIConfigBase::ParseByJSONFile)
      .def("parse_by_json_str", &UNIConfigBase::ParseByJSONStr)
      .def_readwrite("config_root_dir", &UNIConfigBase::config_root_dir);
  py::class_<ProfilerConfig, UNIConfigBase>(m, "ProfilerConfig")
      .def(py::init())
      .def("parse_by_json_str", &ProfilerConfig::ParseByJSONStr)
      .def_readwrite("enable_profiling", &ProfilerConfig::enable_profiling)
      .def_readwrite("enable_tracing", &ProfilerConfig::enable_tracing)
      .def_readwrite("trace_event_capacity", &ProfilerConfig::trace_event_capacity);
  py::class_<UNIModuleConfig, UNIConfigBase>(m, "UNIModuleConfig")
      .def(py::init())
      .def("parse_by_json_str", &UNIModuleConfig::ParseByJSONStr)
      .def_readwrite("name", &UNIModuleConfig::name)
      .def_readwrite("parameters", &UNIModuleConfig::parameters)
      .def_readwrite("parallelism", &UNIModuleConfig::parallelism)
      .def_readwrite("max_input_queue_size", &UNIModuleConfig::max_input_queue_size)
      .def_readwrite("class_name", &UNIModuleConfig::class_name)
      .def_readwrite("next", &UNIModuleConfig::next);
  py::class_<UNISubgraphConfig, UNIConfigBase>(m, "UNISubgraphConfig")
      .def(py::init())
      .def("parse_by_json_str", &UNISubgraphConfig::ParseByJSONStr)
      .def_readwrite("name", &UNISubgraphConfig::name)
      .def_readwrite("config_path", &UNISubgraphConfig::config_path)
      .def_readwrite("next", &UNISubgraphConfig::next);
  py::class_<UNIGraphConfig, UNIConfigBase>(m, "UNIGraphConfig")
      .def(py::init())
      .def("parse_by_json_str", &UNIGraphConfig::ParseByJSONStr)
      .def_readwrite("name", &UNIGraphConfig::name)
      .def_readwrite("profiler_config", &UNIGraphConfig::profiler_config)
      .def_readwrite("module_configs", &UNIGraphConfig::module_configs)
      .def_readwrite("subgraph_configs", &UNIGraphConfig::subgraph_configs);
  m.def("get_path_relative_to_config_file", &GetPathRelativeToTheJSONFile);
}

}  // namespace unistream

