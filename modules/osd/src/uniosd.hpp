/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
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

#ifndef _UNIOSD_HPP_
#define _UNIOSD_HPP_

#include <opencv2/imgproc/imgproc.hpp>

#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "uniedk_osd.h"
#include "unistream_frame_va.hpp"
#include "osd.hpp"
#include "osd_handler.hpp"

using DrawInfo = unistream::OsdHandler::DrawInfo;

constexpr uint32_t kOsdBlockSize = 64 * 1024;
constexpr uint32_t kOsdBlockNum = 32;
constexpr uint32_t kMaxRectNum = 20;
constexpr uint32_t kMaxTextNum = kOsdBlockNum;

namespace unistream {

class UniFont;

class UniOsd {
 public:
  UniOsd() = delete;
  explicit UniOsd(const std::vector<std::string> &labels);
  ~UniOsd() {
    std::unique_lock<std::mutex> lk(pool_mutex_);
    if (hw_accel_) mempool_.DestroyPool(5000);
  }

  inline void SetTextScale(float scale) { text_scale_ = scale; }
  inline void SetTextThickness(float thickness) { text_thickness_ = thickness; }
  inline void SetBoxThickness(float thickness) { box_thickness_ = thickness; }
  inline void SetSecondaryLabels(std::vector<std::string> labels) { secondary_labels_ = labels; }
  inline void SetUniFont(std::shared_ptr<UniFont> uni_font) { uni_font_ = uni_font; }
  inline void SetHwAccel(bool hw_accel) {
    hw_accel_ = hw_accel;
    if (hw_accel_) InitMempool();
  }

  void DrawLabel(UNIDataFramePtr frame, const UNIObjsVec &objects, std::vector<std::string> attr_keys = {}) /*const*/;
  void DrawLabel(UNIDataFramePtr frame, const std::vector<DrawInfo> &info) /*const*/;
  void DrawLogo(UNIDataFramePtr frame, std::string logo) /*const*/;
  void update_vframe(UNIDataFramePtr frame);

 private:
  std::pair<cv::Point, cv::Point> GetBboxCorner(const unistream::UNIInferObject &object, int img_width,
                                                int img_height) const;
  bool LabelIsFound(const int &label_id) const;
  int GetLabelId(const std::string &label_id_str) const;
  void DrawBox(UNIDataFramePtr frame, const cv::Point &top_left, const cv::Point &bottom_right,
               const cv::Scalar &color);  // const;
  void DrawText(UNIDataFramePtr frame, const cv::Point &bottom_left, const std::string &text, const cv::Scalar &color,
                float scale = 1, int *text_height = nullptr, bool down = true);  // const;
  int CalcThickness(int image_width, float thickness) const {
    int result = thickness * image_width / 300;
    if (result <= 0) result = 1;
    return result;
  }
  double CalcScale(int image_width, float scale) const { return scale * image_width / 1000; }

  float text_scale_ = 1;
  float text_thickness_ = 1;
  float box_thickness_ = 1;
  std::vector<std::string> labels_;
  std::vector<std::string> secondary_labels_;
  std::vector<cv::Scalar> colors_;
  int font_ = cv::FONT_HERSHEY_SIMPLEX;
  std::shared_ptr<UniFont> uni_font_;
  bool hw_accel_ = false;

 private:
  uniedk::BufPool mempool_;
  std::mutex pool_mutex_;

  int InitMempool() {
    std::unique_lock<std::mutex> lk(pool_mutex_);
    UniedkBufSurfaceCreateParams create_params;
    memset(&create_params, 0, sizeof(create_params));
    create_params.device_id = 0;  // TODO(gaoyujia)
    create_params.batch_size = 1;
    create_params.size = kOsdBlockSize;
    create_params.mem_type = UNIEDK_BUF_MEM_UNIFIED;
    mempool_.CreatePool(&create_params, kOsdBlockNum);
    return 0;
  }
  uniedk::BufSurfWrapperPtr GetMem(size_t nSize) {
    std::unique_lock<std::mutex> lk(pool_mutex_);
    uniedk::BufSurfWrapperPtr surfPtr = nullptr;
    if (nSize <= kOsdBlockSize) {
      surfPtr = mempool_.GetBufSurfaceWrapper(0);
    }

    if (!surfPtr) {
      UniedkBufSurfaceCreateParams create_params;
      memset(&create_params, 0, sizeof(create_params));
      create_params.device_id = 0;
      create_params.batch_size = 1;
      create_params.size = nSize;
      create_params.mem_type = UNIEDK_BUF_MEM_UNIFIED;
      UniedkBufSurface *surf = nullptr;
      if (UniedkBufSurfaceCreate(&surf, &create_params) < 0) {
        LOGE(OSD) << "GetMem(): Create BufSurface failed";
        return nullptr;
      }
      surfPtr = std::make_shared<uniedk::BufSurfaceWrapper>(surf);
    }

    memset(surfPtr->GetMappedData(0), 0, nSize);
    return surfPtr;
  }

 private:
  struct BBoxInfo {
    cv::Point top_left;
    cv::Point bottom_right;
    cv::Scalar color;
    int thickness = 1;
    int lineType = 8;
    int shift = 0;
  };
  UniedkOsdRectParams rectParams_[kMaxRectNum];
  uint32_t rect_num_ = 0;
  void DoDrawRect(UNIDataFramePtr frame, BBoxInfo *info, bool last = false);

  struct TextBackground {
    cv::Point top_left;
    cv::Point bottom_right;
    cv::Scalar color;
  };
  UniedkOsdRectParams rectBgParams_[kMaxRectNum];
  uint32_t rectBg_num_ = 0;
  void DoFillRect(UNIDataFramePtr frame, TextBackground *info, bool last = false);

  struct TextInfo {
    cv::Size size;
    uniedk::BufSurfWrapperPtr bitmap;
    cv::Point left_bottom;
    cv::Scalar bg_color;
    TextInfo(cv::Size size, uniedk::BufSurfWrapperPtr bitmap, cv::Point left_bottom, cv::Scalar bg_color)
        : size(size), bitmap(bitmap), left_bottom(left_bottom), bg_color(bg_color) {}
  };
  std::vector<TextInfo> texts;
  UniedkOsdBitmapParams bitmapParams_[kMaxTextNum];
  void DoDrawBitmap(UNIDataFramePtr frame, TextInfo *info, bool last = false);
};  // class UniOsd

}  // namespace unistream

#endif  // _UNIOSD_HPP_
