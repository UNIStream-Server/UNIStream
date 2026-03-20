/*************************************************************************
 * Copyright (C) [2021] by Cambricon, Inc. All rights reserved
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

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "glog/logging.h"

#include "pybind11/numpy.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "uniedk_buf_surface.h"
#include "uniedk_buf_surface_util.hpp"
#include "uniis/infer_server.h"
#include "uniis/shape.h"
#include "uniis/processor.h"

#include "common_wrapper.hpp"

namespace py = pybind11;

namespace infer_server {

uniedk::BufSurfWrapperPtr MatToBufSurface(cv::Mat input, UniedkBufSurfaceColorFormat pixel_format) {
  UniedkBufSurface* surf = nullptr;
  UniedkBufSurfaceCreateParams params;
  memset(&params, 0, sizeof(UniedkBufSurfaceCreateParams));
  params.batch_size = 1;
  params.color_format = pixel_format;
  params.height = input.rows;
  params.width = input.cols;
  params.force_align_1 = true;
  params.mem_type = UNIEDK_BUF_MEM_SYSTEM;

  int chn = 3;
  switch (pixel_format) {
    case UNIEDK_BUF_COLOR_FORMAT_RGB:
    case UNIEDK_BUF_COLOR_FORMAT_BGR:
      chn = 3;
      break;
    case UNIEDK_BUF_COLOR_FORMAT_RGBA:
    case UNIEDK_BUF_COLOR_FORMAT_BGRA:
    case UNIEDK_BUF_COLOR_FORMAT_ARGB:
    case UNIEDK_BUF_COLOR_FORMAT_ABGR:
      chn = 4;
      break;
    case UNIEDK_BUF_COLOR_FORMAT_GRAY8:
      chn = 1;
      break;
    case UNIEDK_BUF_COLOR_FORMAT_NV12:
    case UNIEDK_BUF_COLOR_FORMAT_NV21:
      params.height = input.rows / 3 * 2;
      chn = 1;
      break;
    default:
      LOG(ERROR) << "[EasyDK InferServer] [PythonAPI] [MatToBufSurface] Unsupported pixel format.";
      return nullptr;
  }

  UniedkBufSurfaceCreate(&surf, &params);

  memcpy(surf->surface_list[0].data_ptr, input.data, input.rows * input.cols * chn);
  auto buf_surf = std::make_shared<uniedk::BufSurfaceWrapper>(surf);
  return buf_surf;
}

void PackageWrapper(const py::module& m) {
  // Package
  py::class_<Package, std::shared_ptr<Package>>(m, "Package")
      .def(py::init([](uint32_t data_num, const std::string& tag) { return Package::Create(data_num, tag); }),
          py::arg("data_num"), py::arg("tag") = "")
      .def_readwrite("data", &Package::data)
      .def_readwrite("tag", &Package::tag)
      .def_readwrite("perf", &Package::perf);

  // Input
  py::class_<PreprocInput>(*m, "PreprocInput")
      .def(py::init())
      .def_readwrite("surf", &PreprocInput::surf)
      .def_readwrite("has_bbox", &PreprocInput::has_bbox)
      .def_readwrite("bbox", &PreprocInput::bbox);

  // bbox
  py::class_<UNIInferBoundingBox>(*m, "UNIInferBoundingBox")
      .def(py::init<float, float, float, float>(),
          py::arg("x") = 0.0, py::arg("y") = 0.0 , py::arg("w") = 0.0, py::arg("h") = 0.0)
      .def_readwrite("x", &UNIInferBoundingBox::x)
      .def_readwrite("y", &UNIInferBoundingBox::y)
      .def_readwrite("w", &UNIInferBoundingBox::w)
      .def_readwrite("h", &UNIInferBoundingBox::h);

  // ModelIO
  py::class_<ModelIO, std::shared_ptr<ModelIO>>(m, "ModelIO")
      .def(py::init<>())
      .def_readwrite("surfs", &ModelIO::surfs)
      .def_readwrite("shapes", &ModelIO::shapes);


  // InferData
  py::class_<InferData, std::shared_ptr<InferData>>(m, "InferData")
      .def(py::init<>())
      .def("has_value", &InferData::HasValue)
      // Custom dictionary
      .def("set",
          [](InferData* data, py::dict dict) {
            std::shared_ptr<py::dict> dict_ptr = std::shared_ptr<py::dict>(new py::dict(), [](py::dict* t) {
              // py::dict destruct in c++ thread without gil resource, it is important to get gil
              py::gil_scoped_acquire gil;
              delete t;
            });
            (*dict_ptr) = dict;
            data->Set(dict_ptr);
          })
      .def("get_dict", [](InferData* data) { return *(data->Get<std::shared_ptr<py::dict>>()); })
      .def("set",
          [](InferData* data, PreprocInput input) {
            data->Set(input);
          })
      .def("get_preproc_input",
          [](InferData* data) {
            return data->GetLref<PreprocInput>();
          }, py::return_value_policy::reference)
      // set array (cv mat)
      .def("set",
          [](InferData* data, py::array img_array) {
            cv::Mat cv_img = ArrayToMat(img_array);
            data->Set(std::move(cv_img));
          })
      .def("get_array", [](InferData* data) { return MatToArray(data->GetLref<cv::Mat>()); },
          py::return_value_policy::reference)
      // get ModelIO
      .def("get_model_io", [](InferData* data) { return std::reference_wrapper<ModelIO>(data->GetLref<ModelIO>()); },
          py::return_value_policy::reference)
      .def("set_user_data",
          [](InferData* data, py::dict dict) {
            std::shared_ptr<py::dict> dict_ptr = std::shared_ptr<py::dict>(new py::dict(), [](py::dict* t) {
              // py::dict destruct in c++ thread without gil resource, it is important to get gil
              py::gil_scoped_acquire gil;
              delete t;
            });
            (*dict_ptr) = dict;
            data->SetUserData(dict_ptr);
          })
      .def("get_user_data", [](InferData* data) { return *(data->GetUserData<std::shared_ptr<py::dict>>()); });
}

void BufferSurfaceWrapper(py::module* m) {
  py::enum_<UniedkBufSurfaceColorFormat>(*m, "UniedkBufSurfaceColorFormat")
      .value("INVALID", UniedkBufSurfaceColorFormat::UNIEDK_BUF_COLOR_FORMAT_INVALID)
      .value("GRAY8", UniedkBufSurfaceColorFormat::UNIEDK_BUF_COLOR_FORMAT_GRAY8)
      .value("YUV420", UniedkBufSurfaceColorFormat::UNIEDK_BUF_COLOR_FORMAT_YUV420)
      .value("NV12", UniedkBufSurfaceColorFormat::UNIEDK_BUF_COLOR_FORMAT_NV12)
      .value("NV21", UniedkBufSurfaceColorFormat::UNIEDK_BUF_COLOR_FORMAT_NV21)
      .value("ARGB", UniedkBufSurfaceColorFormat::UNIEDK_BUF_COLOR_FORMAT_ARGB)
      .value("ABGR", UniedkBufSurfaceColorFormat::UNIEDK_BUF_COLOR_FORMAT_ABGR)
      .value("RGB", UniedkBufSurfaceColorFormat::UNIEDK_BUF_COLOR_FORMAT_RGB)
      .value("BGR", UniedkBufSurfaceColorFormat::UNIEDK_BUF_COLOR_FORMAT_BGR)
      .value("BGRA", UniedkBufSurfaceColorFormat::UNIEDK_BUF_COLOR_FORMAT_BGRA)
      .value("RGBA", UniedkBufSurfaceColorFormat::UNIEDK_BUF_COLOR_FORMAT_RGBA)
      .value("ARGB1555", UniedkBufSurfaceColorFormat::UNIEDK_BUF_COLOR_FORMAT_ARGB1555)
      .value("TENSOR", UniedkBufSurfaceColorFormat::UNIEDK_BUF_COLOR_FORMAT_TENSOR)
      .value("LAST", UniedkBufSurfaceColorFormat::UNIEDK_BUF_COLOR_FORMAT_LAST);

  py::enum_<UniedkBufSurfaceMemType>(*m, "UniedkBufSurfaceMemType")
      .value("DEFAULT", UniedkBufSurfaceMemType::UNIEDK_BUF_MEM_DEFAULT)
      .value("DEVICE", UniedkBufSurfaceMemType::UNIEDK_BUF_MEM_DEVICE)
      .value("PINNED", UniedkBufSurfaceMemType::UNIEDK_BUF_MEM_PINNED)
      .value("UNIFIED", UniedkBufSurfaceMemType::UNIEDK_BUF_MEM_UNIFIED)
      .value("UNIFIED_CACHED", UniedkBufSurfaceMemType::UNIEDK_BUF_MEM_UNIFIED_CACHED)
      .value("VB", UniedkBufSurfaceMemType::UNIEDK_BUF_MEM_VB)
      .value("VB_CACHED", UniedkBufSurfaceMemType::UNIEDK_BUF_MEM_VB_CACHED)
      .value("SYSTEM", UniedkBufSurfaceMemType::UNIEDK_BUF_MEM_SYSTEM);

  py::class_<UniedkBufSurfacePlaneParams>(*m, "UniedkBufSurfacePlaneParams")
      .def(py::init())
      .def_readwrite("num_planes", &UniedkBufSurfacePlaneParams::num_planes)
      .def_property("width",
          [](const UniedkBufSurfacePlaneParams& params) {
            return py::array_t<uint32_t>({UNIEDK_BUF_MAX_PLANES}, {sizeof(uint32_t)}, params.width);
          },
          [](std::shared_ptr<UniedkBufSurfacePlaneParams> params, py::array_t<uint32_t> width) {
            {
              py::gil_scoped_acquire gil;
              py::buffer_info width_buf = width.request();
              int size = std::min(static_cast<int>(width_buf.size), UNIEDK_BUF_MAX_PLANES);
              memcpy(params->width, width_buf.ptr, size * sizeof(uint32_t));
            }
          })
      .def_property("height",
          [](const UniedkBufSurfacePlaneParams& params) {
            return py::array_t<uint32_t>({UNIEDK_BUF_MAX_PLANES}, {sizeof(uint32_t)}, params.height);
          },
          [](std::shared_ptr<UniedkBufSurfacePlaneParams> params, py::array_t<uint32_t> height) {
            {
              py::gil_scoped_acquire gil;
              py::buffer_info height_buf = height.request();
              int size = std::min(static_cast<int>(height_buf.size), UNIEDK_BUF_MAX_PLANES);
              memcpy(params->height, height_buf.ptr, size * sizeof(uint32_t));
            }
          })
      .def_property("pitch",
          [](const UniedkBufSurfacePlaneParams& params) {
            return py::array_t<uint32_t>({UNIEDK_BUF_MAX_PLANES}, {sizeof(uint32_t)}, params.pitch);
          },
          [](std::shared_ptr<UniedkBufSurfacePlaneParams> params, py::array_t<uint32_t> pitch) {
            {
              py::gil_scoped_acquire gil;
              py::buffer_info pitch_buf = pitch.request();
              int size = std::min(static_cast<int>(pitch_buf.size), UNIEDK_BUF_MAX_PLANES);
              memcpy(params->pitch, pitch_buf.ptr, size * sizeof(uint32_t));
            }
          })
      .def_property("offset",
          [](const UniedkBufSurfacePlaneParams& params) {
            return py::array_t<uint32_t>({UNIEDK_BUF_MAX_PLANES}, {sizeof(uint32_t)}, params.offset);
          },
          [](std::shared_ptr<UniedkBufSurfacePlaneParams> params, py::array_t<uint32_t> offset) {
            {
              py::gil_scoped_acquire gil;
              py::buffer_info offset_buf = offset.request();
              int size = std::min(static_cast<int>(offset_buf.size), UNIEDK_BUF_MAX_PLANES);
              memcpy(params->offset, offset_buf.ptr, size * sizeof(uint32_t));
            }
          })
      .def_property("psize",
          [](const UniedkBufSurfacePlaneParams& params) {
            return py::array_t<uint32_t>({UNIEDK_BUF_MAX_PLANES}, {sizeof(uint32_t)}, params.psize);
          },
          [](std::shared_ptr<UniedkBufSurfacePlaneParams> params, py::array_t<uint32_t> psize) {
            {
              py::gil_scoped_acquire gil;
              py::buffer_info psize_buf = psize.request();
              int size = std::min(static_cast<int>(psize_buf.size), UNIEDK_BUF_MAX_PLANES);
              memcpy(params->psize, psize_buf.ptr, size * sizeof(uint32_t));
            }
          })
      .def_property("bytes_per_pix",
          [](const UniedkBufSurfacePlaneParams& params) {
            return py::array_t<uint32_t>({UNIEDK_BUF_MAX_PLANES}, {sizeof(uint32_t)}, params.bytes_per_pix);
          },
          [](std::shared_ptr<UniedkBufSurfacePlaneParams> params, py::array_t<uint32_t> bytes_per_pix) {
            {
              py::gil_scoped_acquire gil;
              py::buffer_info bytes_per_pix_buf = bytes_per_pix.request();
              int size = std::min(static_cast<int>(bytes_per_pix_buf.size), UNIEDK_BUF_MAX_PLANES);
              memcpy(params->bytes_per_pix, bytes_per_pix_buf.ptr, size * sizeof(uint32_t));
            }
          });

  py::class_<UniedkBufSurfaceCreateParams, std::shared_ptr<UniedkBufSurfaceCreateParams>>(*m,
      "UniedkBufSurfaceCreateParams")
      .def(py::init())
      .def_readwrite("mem_type", &UniedkBufSurfaceCreateParams::mem_type)
      .def_readwrite("device_id", &UniedkBufSurfaceCreateParams::device_id)
      .def_readwrite("width", &UniedkBufSurfaceCreateParams::width)
      .def_readwrite("height", &UniedkBufSurfaceCreateParams::height)
      .def_readwrite("color_format", &UniedkBufSurfaceCreateParams::color_format)
      .def_readwrite("size", &UniedkBufSurfaceCreateParams::size)
      .def_readwrite("batch_size", &UniedkBufSurfaceCreateParams::batch_size)
      .def_readwrite("force_align_1", &UniedkBufSurfaceCreateParams::force_align_1);

  py::class_<UniedkBufSurfaceParams, std::shared_ptr<UniedkBufSurfaceParams>>(*m, "UniedkBufSurfaceParams")
      .def(py::init())
      .def_readwrite("width", &UniedkBufSurfaceParams::width)
      .def_readwrite("height", &UniedkBufSurfaceParams::height)
      .def_readwrite("pitch", &UniedkBufSurfaceParams::pitch)
      .def_readwrite("color_format", &UniedkBufSurfaceParams::color_format)
      .def_readwrite("data_size", &UniedkBufSurfaceParams::data_size)
      .def_readwrite("plane_params", &UniedkBufSurfaceParams::plane_params)
      .def("copy_from",
           [](std::shared_ptr<UniedkBufSurfaceParams> params, py::array buf_array, size_t copy_size) {
             if (!copy_size) copy_size = GetTotalSize(buf_array);
             void* cpu_src = const_cast<void*>(buf_array.data());
			 axclrtMemcpy(params->data_ptr, cpu_src, copy_size, AXCL_MEMCPY_HOST_TO_DEVICE);
           },
           py::arg("src"), py::arg("size") = 0)
      .def("copy_to",
           [](std::shared_ptr<UniedkBufSurfaceParams> params, py::array* buf_array, size_t copy_size) {
             if (!copy_size) copy_size = GetTotalSize(*buf_array);
             void* dst = const_cast<void*>(buf_array->data());
			 axclrtMemcpy(dst, params->data_ptr, copy_size, AXCL_MEMCPY_DEVICE_TO_HOST);
           },
           py::arg("dst"), py::arg("size") = 0);

  py::class_<UniedkBufSurface, std::shared_ptr<UniedkBufSurface>>(*m, "UniedkBufSurface")
      .def(py::init())
      .def_readwrite("mem_type", &UniedkBufSurface::mem_type)
      .def_readwrite("device_id", &UniedkBufSurface::device_id)
      .def_readwrite("batch_size", &UniedkBufSurface::batch_size)
      .def_readwrite("num_filled", &UniedkBufSurface::num_filled)
      .def_readwrite("is_contiguous", &UniedkBufSurface::is_contiguous)
      .def("get_surface_list",
          [](std::shared_ptr<UniedkBufSurface> buf_surf, int index) {
            return buf_surf->surface_list[index];
          })
      .def("set_surface_list",
          [](std::shared_ptr<UniedkBufSurface> buf_surf, UniedkBufSurfaceParams params, int index) {
            buf_surf->surface_list[index] = params;
          })
      .def("get_buffer",
          [](std::shared_ptr<UniedkBufSurface> buf_surf, int batch_idx) {
            if (batch_idx > 0 && batch_idx < static_cast<int>(buf_surf->batch_size)) {
              return buf_surf->surface_list[batch_idx];
            }
            return UniedkBufSurfaceParams();
          }, py::arg("batch_idx") = 0)
      .def("set_buffer",
          [](std::shared_ptr<UniedkBufSurface> buf_surf, UniedkBufSurfaceParams buffer, int batch_idx) {
            if (batch_idx > 0 && batch_idx < static_cast<int>(buf_surf->batch_size)) {
              buf_surf->surface_list[batch_idx] = buffer;
            }
          }, py::arg("buffer"), py::arg("batch_idx") = 0)
      .def_readwrite("pts", &UniedkBufSurface::pts);

  m->def("uniedk_buf_surface_create",
      [](std::shared_ptr<UniedkBufSurfaceCreateParams> params) {
        UniedkBufSurface* surf = nullptr;
        UniedkBufSurfaceCreate(&surf, params.get());
        std::shared_ptr<UniedkBufSurface> surf_tmp(surf, [](UniedkBufSurface *p) { });
        return surf_tmp;
      });
  m->def("uniedk_buf_surface_destroy",
      [](std::shared_ptr<UniedkBufSurface> surf) {
        UniedkBufSurfaceDestroy(surf.get());
        surf.reset();
      });
  m->def("uniedk_buf_surface_copy", &UniedkBufSurfaceCopy);
  m->def("uniedk_buf_surface_memset", &UniedkBufSurfaceMemSet);

  py::class_<uniedk::BufSurfaceWrapper, std::shared_ptr<uniedk::BufSurfaceWrapper>>(*m, "UniedkBufSurfaceWrapper")
      .def(py::init(
          [](std::shared_ptr<UniedkBufSurface> buf_surf) {
            return std::make_shared<uniedk::BufSurfaceWrapper>(buf_surf.get(), true);
          }))
      .def("get_buf_surface",
          [](std::shared_ptr<uniedk::BufSurfaceWrapper> wrapper) {
            std::shared_ptr<UniedkBufSurface> buf_surf(wrapper->GetBufSurface(), [](UniedkBufSurface *p) { });
            return buf_surf;
          })
      .def("get_surface_params",
          [](std::shared_ptr<uniedk::BufSurfaceWrapper> wrapper, uint32_t batch_idx) {
            std::shared_ptr<UniedkBufSurfaceParams> buf_surf_params(wrapper->GetSurfaceParams(batch_idx),
                                                                   [](UniedkBufSurfaceParams *p) { });
            return buf_surf_params;
          }, py::arg("batch_idx") = 0)
      .def("get_num_filled", &uniedk::BufSurfaceWrapper::GetNumFilled)
      .def("get_color_format", &uniedk::BufSurfaceWrapper::GetColorFormat)
      .def("get_width", &uniedk::BufSurfaceWrapper::GetWidth)
      .def("get_height", &uniedk::BufSurfaceWrapper::GetHeight)
      .def("get_stride", &uniedk::BufSurfaceWrapper::GetStride)
      .def("get_plane_num", &uniedk::BufSurfaceWrapper::GetPlaneNum)
      .def("get_plane_bytes", &uniedk::BufSurfaceWrapper::GetPlaneBytes)
      .def("get_device_id", &uniedk::BufSurfaceWrapper::GetDeviceId)
      .def("get_mem_type", &uniedk::BufSurfaceWrapper::GetMemType)
      .def("get_pts", &uniedk::BufSurfaceWrapper::GetPts)
      .def("set_pts", &uniedk::BufSurfaceWrapper::SetPts)
      .def("get_data",
          [](std::shared_ptr<uniedk::BufSurfaceWrapper> wrapper, uint32_t plane_idx, uint32_t batch_idx,
             DataType dtype) {
            if (wrapper->GetBufSurface()->mem_type != UNIEDK_BUF_MEM_SYSTEM) {
              return py::array();
            }
            void* data = wrapper->GetData(plane_idx, batch_idx);
            size_t data_num = static_cast<size_t>(wrapper->GetSurfaceParams(batch_idx)->data_size/ GetTypeSize(dtype));
            if (!data_num) {
              data_num =  static_cast<size_t>(wrapper->GetPlaneBytes(plane_idx) / GetTypeSize(dtype));
            }
            return PointerToArray(data, {data_num}, dtype);
          }, py::arg("plane_idx") = 0, py::arg("batch_idx") = 0, py::arg("dtype") = DataType::UINT8)
      .def("get_host_data",
          [](std::shared_ptr<uniedk::BufSurfaceWrapper> wrapper, uint32_t plane_idx, uint32_t batch_idx,
             DataType dtype) -> py::array {
            size_t data_num;
            void* data = wrapper->GetHostData(plane_idx, batch_idx);
            if (wrapper->GetPlaneNum() == 0) {
              data_num = static_cast<size_t>(wrapper->GetSurfaceParams(batch_idx)->data_size/ GetTypeSize(dtype));
              return PointerToArray(data, {data_num}, dtype);
            }
            if (wrapper->GetPlaneNum() <= plane_idx) {
              LOG(ERROR) << "[EasyDK InferServer] [PythonAPI] [get_host_data] Unsupported plane_idx.";
              return py::array();
            }
            if (wrapper->GetWidth() && wrapper->GetHeight()) {
              uint32_t stride =
                  wrapper->GetStride(plane_idx) ? wrapper->GetStride(plane_idx) : wrapper->GetWidth();
              uint32_t height = wrapper->GetHeight();
              uint32_t chn;
              switch (wrapper->GetColorFormat()) {
                case UNIEDK_BUF_COLOR_FORMAT_YUV420:
                  chn = 1;
                  if (plane_idx != 0) {
                    height /= 2;
                  }
                  break;
                case UNIEDK_BUF_COLOR_FORMAT_NV12:
                case UNIEDK_BUF_COLOR_FORMAT_NV21:
                  chn = 1;
                  if (plane_idx != 0) {
                    height /= 2;
                  }
                  break;
                case UNIEDK_BUF_COLOR_FORMAT_GRAY8:
                  chn = 1;
                  break;
                case UNIEDK_BUF_COLOR_FORMAT_RGB:
                case UNIEDK_BUF_COLOR_FORMAT_BGR:
                  chn = 3;
                  stride /= chn;
                  break;
                case UNIEDK_BUF_COLOR_FORMAT_ARGB:
                case UNIEDK_BUF_COLOR_FORMAT_ABGR:
                case UNIEDK_BUF_COLOR_FORMAT_BGRA:
                case UNIEDK_BUF_COLOR_FORMAT_RGBA:
                  chn = 4;
                  stride /= chn;
                  break;
                default:
                  chn = 1;
                  break;
              }
              return PointerToArray(data, {height, stride, chn}, dtype);
            }
            data_num =  static_cast<size_t>(wrapper->GetPlaneBytes(plane_idx) / GetTypeSize(dtype));
            return PointerToArray(data, {data_num}, dtype);
          }, py::arg("plane_idx") = 0, py::arg("batch_idx") = 0, py::arg("dtype") = DataType::UINT8)
      .def("sync_host_to_device",
          [](std::shared_ptr<uniedk::BufSurfaceWrapper> wrapper, uint32_t plane_idx, uint32_t batch_idx) {
            wrapper->SyncHostToDevice(plane_idx, batch_idx);
          }, py::arg("plane_idx") = 0, py::arg("batch_idx") = 0);
  m->def("convert_to_buf_surf",
      [](py::array img_array, UniedkBufSurfaceColorFormat pixel_format) {
        cv::Mat cv_img = ArrayToMat(img_array);
        return MatToBufSurface(cv_img, pixel_format);
      }, py::arg("img"), py::arg("pixel_format") = UniedkBufSurfaceColorFormat::UNIEDK_BUF_COLOR_FORMAT_BGR);
}

}  //  namespace infer_server
