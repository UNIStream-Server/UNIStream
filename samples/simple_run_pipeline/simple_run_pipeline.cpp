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

#include <signal.h>
#include <stdio.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gflags/gflags.h"
#include "opencv2/opencv.hpp"
#if (CV_MAJOR_VERSION >= 4)
#include "opencv2/videoio/videoio_c.h"
#endif

#include "uniedk_platform.h"
#include "unistream_frame_va.hpp"
#include "data_source.hpp"
#include "unistream_postproc.hpp"
#include "unistream_preproc.hpp"

DEFINE_string(input_url, "", "video file or images. e.g. /your/path/to/file.mp4, /your/path/to/images/%d.jpg.");
DEFINE_int32(input_num, 1, "input number");
DEFINE_string(how_to_show, "video", "image/video, otherwise do not show");
DEFINE_string(model_path, "", "/your/path/to/model_name.magicmind");
DEFINE_string(model_type, "yolov11", "yolov5/resnet50.");
DEFINE_string(label_path, "", "/your/path/to/label.txt");
DEFINE_string(output_dir, "./", "/your/path/to/output_dir");
DEFINE_int32(output_frame_rate, 25, "output frame rate");
DEFINE_bool(keep_aspect_ratio, false, "keep aspect ratio for image scaling");
DEFINE_string(pad_value, "114, 114, 114", "pad value in model input pixel format order");
DEFINE_string(mean_value, "0, 0, 0", "mean value in model input pixel format order");
DEFINE_string(std, "1.0, 1.0, 1.0", "std in model input pixel format order");
DEFINE_string(model_input_pixel_format, "RGB", "BGR/RGB");
DEFINE_int32(dev_id, 0, "device ordinal index");
DEFINE_int32(codec_id_start, 0, "vdec/venc first id, for CE3226 only");

namespace simple_pipeline {

static std::array<uint32_t, 3> gPadValue;
static std::array<float, 3> gMeanValue;
static std::array<float, 3> gStd;
static infer_server::NetworkInputFormat gFmt;
static bool gMeanStd;

typedef struct BaseObject
{
    cv::Rect_<float> rect;
    int label;
    float prob;
    cv::Point2f landmark[5];
    /* for yolov5-seg */
    cv::Mat mask;
    std::vector<float> mask_feat;
    std::vector<float> kps_feat;
    /* for yolov8-obb */
    float angle;
} BaseObject;

static inline float sigmoid(float x)
{
    return static_cast<float>(1.f / (1.f + exp(-x)));
}

static float softmax(const float* src, float* dst, int length)
{
    const float alpha = *std::max_element(src, src + length);
    float denominator = 0;
    float dis_sum = 0;
    for (int i = 0; i < length; ++i)
    {
        dst[i] = exp(src[i] - alpha);
        denominator += dst[i];
    }
    for (int i = 0; i < length; ++i)
    {
        dst[i] /= denominator;
        dis_sum += i * dst[i];
    }
    return dis_sum;
}

static void generate_proposals_yolov8_native(int stride, const float* feat, float prob_threshold, std::vector<BaseObject>& objects,
											 int letterbox_cols, int letterbox_rows, int cls_num = 80)
{
	int feat_w = letterbox_cols / stride;
	int feat_h = letterbox_rows / stride;
	int reg_max = 16;

	auto feat_ptr = feat;

	std::vector<float> dis_after_sm(reg_max, 0.f);
	for (int h = 0; h <= feat_h - 1; h++)
	{
		for (int w = 0; w <= feat_w - 1; w++)
		{
			// process cls score
			int class_index = 0;
			float class_score = -FLT_MAX;
			for (int s = 0; s < cls_num; s++)
			{
				float score = feat_ptr[s + 4 * reg_max];
				if (score > class_score)
				{
					class_index = s;
					class_score = score;
				}
			}

			float box_prob = sigmoid(class_score);
			if (box_prob > prob_threshold)
			{
				float pred_ltrb[4];
				for (int k = 0; k < 4; k++)
				{
					float dis = softmax(feat_ptr + k * reg_max, dis_after_sm.data(), reg_max);
					pred_ltrb[k] = dis * stride;
				}

				float pb_cx = (w + 0.5f) * stride;
				float pb_cy = (h + 0.5f) * stride;

				float x0 = pb_cx - pred_ltrb[0];
				float y0 = pb_cy - pred_ltrb[1];
				float x1 = pb_cx + pred_ltrb[2];
				float y1 = pb_cy + pred_ltrb[3];

				x0 = std::max(std::min(x0, (float)(letterbox_cols - 1)), 0.f);
				y0 = std::max(std::min(y0, (float)(letterbox_rows - 1)), 0.f);
				x1 = std::max(std::min(x1, (float)(letterbox_cols - 1)), 0.f);
				y1 = std::max(std::min(y1, (float)(letterbox_rows - 1)), 0.f);

				BaseObject obj;
				obj.rect.x = x0;
                obj.rect.y = y0;
                obj.rect.width = x1 - x0;
                obj.rect.height = y1 - y0;
                obj.label = class_index;
                obj.prob = box_prob;

				objects.push_back(obj);
			}

			feat_ptr += (cls_num + 4 * reg_max);
		}
	}
}

template<typename T>
static void qsort_descent_inplace(std::vector<T>& faceobjects, int left, int right)
{
    int i = left;
    int j = right;
    float p = faceobjects[(left + right) / 2].prob;

    while (i <= j)
    {
        while (faceobjects[i].prob > p)
            i++;

        while (faceobjects[j].prob < p)
            j--;

        if (i <= j)
        {
            // swap
            std::swap(faceobjects[i], faceobjects[j]);

            i++;
            j--;
        }
    }
// #pragma omp parallel sections
    {
// #pragma omp section
        {
            if (left < j) qsort_descent_inplace(faceobjects, left, j);
        }
// #pragma omp section
        {
            if (i < right) qsort_descent_inplace(faceobjects, i, right);
        }
    }
}

template<typename T>
static void qsort_descent_inplace(std::vector<T>& faceobjects)
{
    if (faceobjects.empty())
        return;

    qsort_descent_inplace(faceobjects, 0, faceobjects.size() - 1);
}

template<typename T>
static inline float intersection_area(const T& a, const T& b)
{
    cv::Rect_<float> inter = a.rect & b.rect;
    return inter.area();
}

template<typename T>
static void nms_sorted_bboxes(const std::vector<T>& faceobjects, std::vector<int>& picked, float nms_threshold)
{
    picked.clear();

    const int n = faceobjects.size();

    std::vector<float> areas(n);
    for (int i = 0; i < n; i++)
    {
        areas[i] = faceobjects[i].rect.area();
    }

    for (int i = 0; i < n; i++)
    {
        const T& a = faceobjects[i];

        int keep = 1;
        for (int j = 0; j < (int)picked.size(); j++)
        {
            const T& b = faceobjects[picked[j]];

            // intersection over union
            float inter_area = intersection_area(a, b);
            float union_area = areas[i] + areas[picked[j]] - inter_area;
            // float IoU = inter_area / union_area
            if (inter_area / union_area > nms_threshold)
                keep = 0;
        }

        if (keep)
            picked.push_back(i);
    }
}

static void get_out_bbox(std::vector<BaseObject>& proposals, std::vector<BaseObject>& objects, const float nms_threshold, int letterbox_rows, int letterbox_cols, int src_rows, int src_cols)
{
	qsort_descent_inplace(proposals);
	std::vector<int> picked;
	nms_sorted_bboxes(proposals, picked, nms_threshold);

	/* yolov5 draw the result */
	float scale_letterbox;
	int resize_rows;
	int resize_cols;
	if ((letterbox_rows * 1.0 / src_rows) < (letterbox_cols * 1.0 / src_cols))
	{
		scale_letterbox = letterbox_rows * 1.0 / src_rows;
	}
	else
	{
		scale_letterbox = letterbox_cols * 1.0 / src_cols;
	}
	resize_cols = int(scale_letterbox * src_cols);
	resize_rows = int(scale_letterbox * src_rows);

	int tmp_h = (letterbox_rows - resize_rows) / 2;
	int tmp_w = (letterbox_cols - resize_cols) / 2;

	float ratio_x = (float)src_rows / resize_rows;
	float ratio_y = (float)src_cols / resize_cols;

	int count = picked.size();

	objects.resize(count);
	for (int i = 0; i < count; i++)
	{
		objects[i] = proposals[picked[i]];
		
		float x0 = (objects[i].rect.x);
		float y0 = (objects[i].rect.y);
		float x1 = (objects[i].rect.x + objects[i].rect.width);
		float y1 = (objects[i].rect.y + objects[i].rect.height);

		x0 = (x0 - tmp_w) * ratio_x;
		y0 = (y0 - tmp_h) * ratio_y;
		x1 = (x1 - tmp_w) * ratio_x;
		y1 = (y1 - tmp_h) * ratio_y;

		for (int l = 0; l < 5; l++)
		{
			auto lx = objects[i].landmark[l].x;
			auto ly = objects[i].landmark[l].y;
			objects[i].landmark[l] = cv::Point2f((lx - tmp_w) * ratio_x, (ly - tmp_h) * ratio_y);
		}

		x0 = std::max(std::min(x0, (float)(src_cols - 1)), 0.f);
		y0 = std::max(std::min(y0, (float)(src_rows - 1)), 0.f);
		x1 = std::max(std::min(x1, (float)(src_cols - 1)), 0.f);
		y1 = std::max(std::min(y1, (float)(src_rows - 1)), 0.f);

		objects[i].rect.x = x0;
		objects[i].rect.y = y0;
		objects[i].rect.width = x1 - x0;
		objects[i].rect.height = y1 - y0;

        #if 0
		printf("des objects %4.0f %4.0f %4.0f %4.0f\n", 
			   objects[i].rect.x, objects[i].rect.y, 
			   objects[i].rect.x + objects[i].rect.width, objects[i].rect.y + objects[i].rect.height);
        #endif
	}
}

// Init mean values and std. init channel order for color convert(eg. bgr to rgba)
bool InitGlobalValues() {
  if (3 != sscanf(FLAGS_pad_value.c_str(), "%d, %d, %d", &gPadValue[0], &gPadValue[1], &gPadValue[2])) {
    LOGE(SIMPLE_PIPELINE) << "Parse pad value failed. pad value should be the "
      "following format :\"114, 114, 114\"";
    return false;
  }
  if (3 != sscanf(FLAGS_mean_value.c_str(), "%f, %f, %f", &gMeanValue[0], &gMeanValue[1], &gMeanValue[2])) {
    LOGE(SIMPLE_PIPELINE) << "Parse mean value failed. mean value should be the "
      "following format :\"100.2, 100.2, 100.2\"";
    return false;
  }
  if (3 != sscanf(FLAGS_std.c_str(), "%f, %f, %f", &gStd[0], &gStd[1], &gStd[2])) {
    LOGE(SIMPLE_PIPELINE) << "Parse std failed. std should be the following format :\"100.2, 100.2, 100.2\"";
    return false;
  }

  if (abs(gMeanValue[0]) < 1e-6 && abs(gMeanValue[1]) < 1e-6 && abs(gMeanValue[2]) < 1e-6 &&
      abs(gStd[0] - 1) < 1e-6 && abs(gStd[1] - 1) < 1e-6 && abs(gStd[2] - 1) < 1e-6) {
    gMeanStd = false;
  } else {
    gMeanStd = true;
  }

  if (FLAGS_model_input_pixel_format == "RGB") {
    gFmt = infer_server::NetworkInputFormat::RGB;
  } else if (FLAGS_model_input_pixel_format == "BGR") {
    gFmt = infer_server::NetworkInputFormat::BGR;
  } else {
    LOGE(SIMPLE_PIPELINE) << "Parse model input pixel format failed, Must be one of [BGR/RGB], but "
                          << FLAGS_model_input_pixel_format;
    return false;
  }

  if ("yolov11" != FLAGS_model_type && "resnet50" != FLAGS_model_type) return false;
  return true;
}

using UNIFrameInfoSptr = std::shared_ptr<unistream::UNIFrameInfo>;
using SourceHandlerSptr = std::shared_ptr<unistream::SourceHandler>;

// Reflex object, used to do image preprocessing. See parameter named preproc_name in Inferencer module.
class Preprocessor : public unistream::Preproc {
 public:
  int OnTensorParams(const infer_server::UniPreprocTensorParams *params) override;
  int Execute(uniedk::BufSurfWrapperPtr src_buf, uniedk::BufSurfWrapperPtr dst,
              const std::vector<UniedkTransformRect> &src_rects) override;

 private:
  std::mutex mutex_;
  unistream::UniPreprocNetworkInfo info_;
  DECLARE_REFLEX_OBJECT_EX(simple_pipeline::Preprocessor, unistream::Preproc);
};  // class Preprocessor

IMPLEMENT_REFLEX_OBJECT_EX(simple_pipeline::Preprocessor, unistream::Preproc);

// Reflex object, used to do postprocessing. See parameter named postproc_name in Inferencer module.
// Supports classification models and detection models. eg. vgg, resnet, ssd, yolo-vx...
class Postprocessor : public unistream::Postproc {
 public:
  int Execute(const unistream::NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
              const std::vector<unistream::UNIFrameInfoPtr>& packages,
              const unistream::LabelStrings& labels) override;

 private:
  int ExecuteYolov11(const unistream::NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
                    const std::vector<unistream::UNIFrameInfoPtr>& packages,
                    const unistream::LabelStrings& labels);
  int ExecuteResnet50(const unistream::NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
                      const std::vector<unistream::UNIFrameInfoPtr>& packages,
                      const unistream::LabelStrings& labels);
  DECLARE_REFLEX_OBJECT_EX(simple_pipeline::Postprocessor, unistream::Postproc)
};  // class Postprocessor

IMPLEMENT_REFLEX_OBJECT_EX(simple_pipeline::Postprocessor, unistream::Postproc);

// Base class to do visualization
class VisualizerBase {
 public:
  virtual ~VisualizerBase() {}
  virtual void OnStart() {}
  virtual void OnFrame(const UNIFrameInfoSptr& frame_info) = 0;
  virtual void OnStop() {}
};  // class Visualizer

// Save the picture with the detection result or classification result to the disk.
class ImageSaver : public VisualizerBase {
 public:
  explicit ImageSaver(const std::string& stream_id) : stream_id_(stream_id) {}
  void OnFrame(const UNIFrameInfoSptr& frame_info) override;

 private:
  std::string stream_id_ = "";
  uint64_t frame_index_ = 0;
};  // class ImageSaver

// Encode pictures with the detection result or classification result into avi video file.
class VideoSaver : public VisualizerBase {
 public:
  explicit VideoSaver(int frame_rate, const std::string& stream_id)
      : fr_(frame_rate), stream_id_(stream_id) {}
  void OnStart() override;
  void OnFrame(const UNIFrameInfoSptr& frame_info) override;
  void OnStop() override;

 private:
  int fr_ = 25;
  std::string stream_id_ = "";
  cv::VideoWriter writer_;
  cv::Size video_size_ = {1920, 1080};
};  // class VideoSaver

// Use OpenCV to show the picture with the detection result or classification result.
class OpencvDisplayer : public VisualizerBase {
 public:
  explicit OpencvDisplayer(int frame_rate, const std::string& stream_id)
      : fr_(frame_rate), stream_id_(stream_id) {}
  void OnStart() override;
  void OnFrame(const UNIFrameInfoSptr& frame_info) override;
  void OnStop() override;

 private:
  int fr_ = 25;
  std::string stream_id_ = "";
  std::chrono::steady_clock::time_point last_show_time_;
};  // class OpencvDisplayer

// Pipeline runner.
// This class will show you how to build a pipeline,
// how to load images or videos into the pipeline and perform decoding, detection, and classification tasks
// and how to get the pipeline execution results.
class SimplePipelineRunner : public unistream::Pipeline, public unistream::StreamMsgObserver,
                             public unistream::IModuleObserver {
 public:
  SimplePipelineRunner();
  int StartPipeline();
  int AddStream(const std::string& url, const std::string& stream_id, VisualizerBase* visualizer = nullptr);
  int RemoveStream(const std::string& stream_id);
  void WaitPipelineDone();
  void ForceStop() {
    std::unique_lock<std::mutex> lk(mutex_);
    force_exit_.store(true);
  }

 private:
  void Notify(UNIFrameInfoSptr frame_info) override;
  void Update(const unistream::StreamMsg& msg) override;

 private:
  void IncreaseStream(std::string stream_id) {
    if (stream_set_.find(stream_id) != stream_set_.end()) {
      LOGF(SIMPLE_PIPELINE) << "IncreaseStream() The stream is ongoing []" << stream_id;
    }
    stream_set_.insert(stream_id);
    if (stop_) stop_ = false;
  }

 private:
  unistream::DataSource* source_ = nullptr;
  std::unordered_map<std::string, VisualizerBase*> visualizer_map_;
  std::atomic<bool> stop_{false};
  std::set<std::string> stream_set_;
  std::condition_variable wakener_;
  mutable std::mutex mutex_;
  std::atomic<bool> force_exit_{false};
};  // class SimplePipelineRunner

int Preprocessor::OnTensorParams(const infer_server::UniPreprocTensorParams *params) {
  std::unique_lock<std::mutex> lk(mutex_);
  if (GetNetworkInfo(params, &info_) < 0) {
    LOGE(SIMPLE_PIPELINE) << "[Preproc] get network information failed.";
    return -1;
  }

  VLOG1(SIMPLE_PIPELINE) << "[Preproc] Model input : w = " << info_.w << ", h = " << info_.h << ", c = " << info_.c
                         << ", dtype = " << static_cast<int>(info_.dtype)
                         << ", pixel_format = " << static_cast<int>(info_.format);
  return 0;
}

UniedkBufSurfaceColorFormat GetBufSurfaceColorFormat(infer_server::NetworkInputFormat pix_fmt) {
  switch (pix_fmt) {
    case infer_server::NetworkInputFormat::RGB:
      return UNIEDK_BUF_COLOR_FORMAT_RGB;
    case infer_server::NetworkInputFormat::BGR:
      return UNIEDK_BUF_COLOR_FORMAT_BGR;
    default:
      LOGW(SIMPLE_PIPELINE) << "Unknown input pixel format, use RGB as default";
      return UNIEDK_BUF_COLOR_FORMAT_RGB;
  }
}

int PreprocessCpu(uniedk::BufSurfWrapperPtr src, uniedk::BufSurfWrapperPtr dst,
                  const std::vector<UniedkTransformRect> &src_rects,
                  const unistream::UniPreprocNetworkInfo &info) {
  if (src_rects.size() && src_rects.size() != src->GetNumFilled()) {
    return -1;
  }

  if ((src->GetColorFormat() != UNIEDK_BUF_COLOR_FORMAT_NV12 &&
       src->GetColorFormat() != UNIEDK_BUF_COLOR_FORMAT_NV21) ||
      (gFmt != infer_server::NetworkInputFormat::RGB && gFmt != infer_server::NetworkInputFormat::BGR)) {
    LOGE(SIMPLE_PIPELINE) << "[PreprocessCpu] Unsupported pixel format convertion";
    return -1;
  }

  if (info.dtype == infer_server::DataType::UINT8 && gMeanStd) {
    LOGW(SIMPLE_PIPELINE) << "[PreprocessCpu] not support uint8 with mean std.";
  }

  uint32_t batch_size = src->GetNumFilled();

  UniedkBufSurface* src_buf = src->GetBufSurface();
  UniedkBufSurfaceSyncForCpu(src_buf, -1, -1);
  size_t img_size = info.w * info.h * info.c;
  std::unique_ptr<uint8_t[]> img_tmp = nullptr;

  for (uint32_t batch_idx = 0; batch_idx < batch_size; ++batch_idx) {
    uint8_t *y_plane = static_cast<uint8_t *>(src->GetHostData(0, batch_idx));
    uint8_t *uv_plane = static_cast<uint8_t *>(src->GetHostData(1, batch_idx));
    UniedkTransformRect src_bbox;
    if (src_rects.size()) {
      src_bbox = src_rects[batch_idx];
      // validate bbox
      src_bbox.left -= src_bbox.left & 1;
      src_bbox.top -= src_bbox.top & 1;
      src_bbox.width -= src_bbox.width & 1;
      src_bbox.height -= src_bbox.height & 1;
      while (src_bbox.left + src_bbox.width > src_buf->surface_list[batch_idx].width) src_bbox.width -= 2;
      while (src_bbox.top + src_bbox.height > src_buf->surface_list[batch_idx].height) src_bbox.height -= 2;
    } else {
      src_bbox.left = 0;
      src_bbox.top = 0;
      src_bbox.width = src_buf->surface_list[batch_idx].width;
      src_bbox.height = src_buf->surface_list[batch_idx].height;
    }
    // apply src_buf roi
    int y_stride = src_buf->surface_list[batch_idx].plane_params.pitch[0];
    int uv_stride = src_buf->surface_list[batch_idx].plane_params.pitch[1];
    UniedkBufSurfaceColorFormat src_fmt = src_buf->surface_list[batch_idx].color_format;
    UniedkBufSurfaceColorFormat dst_fmt = GetBufSurfaceColorFormat(gFmt);

    y_plane += src_bbox.left + src_bbox.top * y_stride;
    uv_plane += src_bbox.left + src_bbox.top / 2 * uv_stride;

    void *dst_img = dst->GetHostData(0, batch_idx);

    uint8_t *dst_img_u8, *dst_img_roi;
    UniedkTransformRect dst_bbox;
    if (info.dtype == infer_server::DataType::UINT8) {
      dst_img_u8 = reinterpret_cast<uint8_t *>(dst_img);
    } else if (info.dtype == infer_server::DataType::FP32) {
      img_tmp.reset(new uint8_t[img_size]);
      dst_img_u8 = img_tmp.get();
    } else {
      return -1;
    }

    if (gPadValue[0] == gPadValue[1] && gPadValue[0] == gPadValue[2]) {
      memset(dst_img_u8, gPadValue[0], img_size);
    } else {
      for (uint32_t i = 0; i < info.w * info.h; i++) {
        for (uint32_t c_i = 0; c_i < info.c; c_i++) {
          dst_img_u8[i * info.c + c_i] = gPadValue[c_i];
        }
      }
    }
    if (FLAGS_keep_aspect_ratio) {
      dst_bbox = unistream::KeepAspectRatio(src_bbox.width, src_bbox.height, info.w, info.h);
      // validate bbox
      dst_bbox.left -= dst_bbox.left & 1;
      dst_bbox.top -= dst_bbox.top & 1;
      dst_bbox.width -= dst_bbox.width & 1;
      dst_bbox.height -= dst_bbox.height & 1;

      while (dst_bbox.left + dst_bbox.width > info.w) dst_bbox.width -= 2;
      while (dst_bbox.top + dst_bbox.height > info.h) dst_bbox.height -= 2;

      dst_img_roi = dst_img_u8 + dst_bbox.left * info.c + dst_bbox.top * info.w * info.c;
    } else {
      dst_bbox.left = 0;
      dst_bbox.top = 0;
      dst_bbox.width = info.w;
      dst_bbox.height = info.h;
      dst_img_roi = dst_img_u8;
    }

    unistream::YUV420spToRGBx(y_plane, uv_plane, src_bbox.width, src_bbox.height, y_stride, uv_stride, src_fmt,
                             dst_img_roi, dst_bbox.width, dst_bbox.height, info.w * info.c, dst_fmt);

    if (info.dtype == infer_server::DataType::FP32) {
      float *dst_img_fp32 = reinterpret_cast<float *>(dst_img);
      if (gMeanStd) {
        for (uint32_t i = 0; i < info.w * info.h; i++) {
          for (uint32_t c_i = 0; c_i < info.c; c_i++) {
            dst_img_fp32[i * info.c + c_i] = (dst_img_u8[i * info.c + c_i] - gMeanValue[c_i]) / gStd[c_i];
          }
        }
      } else {
        for (uint32_t i = 0; i < img_size; i++) {
          dst_img_fp32[i] = static_cast<float>(dst_img_u8[i]);
        }
      }
    }
    dst->SyncHostToDevice(-1, batch_idx);
  }
  return 0;
}

UniedkTransformColorFormat GetTransformColorFormat(infer_server::NetworkInputFormat pix_fmt) {
  switch (pix_fmt) {
    case infer_server::NetworkInputFormat::RGB:
      return UNIEDK_TRANSFORM_COLOR_FORMAT_RGB;
    case infer_server::NetworkInputFormat::BGR:
      return UNIEDK_TRANSFORM_COLOR_FORMAT_BGR;
    default:
      LOGW(PREPROC) << "Unknown input pixel format, use RGB as default";
      return UNIEDK_TRANSFORM_COLOR_FORMAT_RGB;
  }
}

UniedkTransformDataType GetTransformDataType(infer_server::DataType dtype) {
  switch (dtype) {
    case infer_server::DataType::UINT8:
      return UNIEDK_TRANSFORM_UINT8;
    case infer_server::DataType::FP32:
      return UNIEDK_TRANSFORM_FLOAT32;
    case infer_server::DataType::FP16:
      return UNIEDK_TRANSFORM_FLOAT16;
    case infer_server::DataType::INT32:
      return UNIEDK_TRANSFORM_INT32;
    case infer_server::DataType::INT16:
      return UNIEDK_TRANSFORM_INT16;
    default:
      LOGW(PREPROC) << "Unknown data type, use UINT8 as default";
      return UNIEDK_TRANSFORM_UINT8;
  }
}

int PreprocessTransform(uniedk::BufSurfWrapperPtr src, uniedk::BufSurfWrapperPtr dst,
                        const std::vector<UniedkTransformRect> &src_rects,
                        const unistream::UniPreprocNetworkInfo &info, infer_server::NetworkInputFormat pix_fmt,
                        bool keep_aspect_ratio = true, int pad_value = 0, bool mean_std = false,
                        std::vector<float> mean = {}, std::vector<float> std = {});

int PreprocessTransform(uniedk::BufSurfWrapperPtr src, uniedk::BufSurfWrapperPtr dst,
                        const std::vector<UniedkTransformRect> &src_rects,
                        const unistream::UniPreprocNetworkInfo &info, infer_server::NetworkInputFormat pix_fmt,
                        bool keep_aspect_ratio, int pad_value, bool mean_std, std::vector<float> mean,
                        std::vector<float> std) {
  if (src_rects.size() && src_rects.size() != src->GetNumFilled()) {
    return -1;
  }
  UniedkBufSurface* src_buf = src->GetBufSurface();
  UniedkBufSurface* dst_buf = dst->GetBufSurface();

  uint32_t batch_size = src->GetNumFilled();
  std::vector<UniedkTransformRect> src_rect(batch_size);
  std::vector<UniedkTransformRect> dst_rect(batch_size);
  UniedkTransformParams params;
  memset(&params, 0, sizeof(params));
  params.transform_flag = 0;
  if (src_rects.size()) {
    params.transform_flag |= UNIEDK_TRANSFORM_CROP_SRC;
    params.src_rect = src_rect.data();
    memset(src_rect.data(), 0, sizeof(UniedkTransformRect) * batch_size);
    for (uint32_t i = 0; i < batch_size; i++) {
      UniedkTransformRect *src_bbox = &src_rect[i];
      *src_bbox = src_rects[i];
      // validate bbox
      src_bbox->left -= src_bbox->left & 1;
      src_bbox->top -= src_bbox->top & 1;
      src_bbox->width -= src_bbox->width & 1;
      src_bbox->height -= src_bbox->height & 1;
      while (src_bbox->left + src_bbox->width > src_buf->surface_list[i].width) src_bbox->width -= 2;
      while (src_bbox->top + src_bbox->height > src_buf->surface_list[i].height) src_bbox->height -= 2;
    }
  }

  if (keep_aspect_ratio) {
    params.transform_flag |= UNIEDK_TRANSFORM_CROP_DST;
    params.dst_rect = dst_rect.data();
    memset(dst_rect.data(), 0, sizeof(UniedkTransformRect) * batch_size);
    for (uint32_t i = 0; i < batch_size; i++) {
      UniedkTransformRect *dst_bbox = &dst_rect[i];
      *dst_bbox = unistream::KeepAspectRatio(src_buf->surface_list[i].width, src_buf->surface_list[i].height,
                                            info.w, info.h);
      // validate bbox
      dst_bbox->left -= dst_bbox->left & 1;
      dst_bbox->top -= dst_bbox->top & 1;
      dst_bbox->width -= dst_bbox->width & 1;
      dst_bbox->height -= dst_bbox->height & 1;

      while (dst_bbox->left + dst_bbox->width > info.w) dst_bbox->width -= 2;
      while (dst_bbox->top + dst_bbox->height > info.h) dst_bbox->height -= 2;
    }
  }

  UniedkTransformMeanStdParams mean_std_params;
  if (mean_std) {
    if (mean.size() < info.c || std.size() < info.c) {
      LOGE(PREPROC) << "[PreprocessTransform] Invalid mean std value size";
      return -1;
    }
    params.transform_flag |= UNIEDK_TRANSFORM_MEAN_STD;
    for (uint32_t c_i = 0; c_i < info.c; c_i++) {
      mean_std_params.mean[c_i] = mean[c_i];
      mean_std_params.std[c_i] = std[c_i];
    }
    params.mean_std_params = &mean_std_params;
  }

  // configure dst_desc
  UniedkTransformTensorDesc dst_desc;
  dst_desc.color_format = GetTransformColorFormat(pix_fmt);
  dst_desc.data_type = GetTransformDataType(info.dtype);
  dst_desc.shape.n = info.n;
  dst_desc.shape.c = info.c;
  dst_desc.shape.h = info.h;
  dst_desc.shape.w = info.w;
  params.dst_desc = &dst_desc;

  UniedkBufSurfaceMemSet(dst_buf, -1, -1, pad_value);
  if (UniedkTransform(src_buf, dst_buf, &params) < 0) {
    LOGE(PREPROC) << "[PreprocessTransform] UniedkTransform failed";
    return -1;
  }

  return 0;
}

int Preprocessor::Execute(uniedk::BufSurfWrapperPtr src, uniedk::BufSurfWrapperPtr dst,
                          const std::vector<UniedkTransformRect> &src_rects) {
  LOGF_IF(SIMPLE_PIPELINE, info_.c != 3) << "[Preproc] model input channel is not equal to 3";
  if (PreprocessTransform(src, dst, src_rects, info_, info_.format, true, 128) != 0) {
    LOGE(SIMPLE_PIPELINE) << "[Preprocessor] preprocess failed.";
    return -1;
  }
  return 0;
}

int Postprocessor::Execute(const unistream::NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
                           const std::vector<unistream::UNIFrameInfoPtr>& packages,
                           const unistream::LabelStrings& labels) {
  if ("yolov11" == FLAGS_model_type) {
    ExecuteYolov11(net_outputs, model_info, packages, labels);
  } else if ("resnet50" == FLAGS_model_type) {
    ExecuteResnet50(net_outputs, model_info, packages, labels);
  } else {
    LOGF(SIMPLE_PIPELINE) << "Never be here.";
  }
  return 0;
}

#define CLIP(x) ((x) < 0 ? 0 : ((x) > 1 ? 1 : (x)))

int Postprocessor::ExecuteYolov11(const unistream::NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
                                 const std::vector<unistream::UNIFrameInfoPtr>& packages,
                                 const unistream::LabelStrings& labels) {
  #if 1
  const char *CLASS_NAMES[] = {
    "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light",
    "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow",
    "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
    "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard", "surfboard",
    "tennis racket", "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple",
    "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch",
    "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote", "keyboard", "cell phone",
    "microwave", "oven", "toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors", "teddy bear",
    "hair drier", "toothbrush"};
  #endif

  int NUM_CLASS = 80;

  printf("OnPostproc start\n");

  for (size_t i = 0; i < net_outputs.size(); i++)
  {
	  if (!net_outputs[i].first->GetHostData(0)) {
	    LOGE(SIMPLE_PIPELINE) << "[PostprocYolov11] Postprocess failed, copy data to host failed.";
	    return -1;
	  }
  }

  infer_server::DimOrder input_order = model_info.InputLayout(0).order;
  auto s = model_info.InputShape(0);
  int model_input_w, model_input_h;
  if (input_order == infer_server::DimOrder::NCHW) {
    model_input_w = s[3];
    model_input_h = s[2];
  } else if (input_order == infer_server::DimOrder::NHWC) {
    model_input_w = s[2];
    model_input_h = s[1];
  } else {
    LOGE(SIMPLE_PIPELINE) << "[PostprocYolov11] Postprocess failed. Unsupported dim order";
    return -1;
  }

  for (size_t batch_idx = 0; batch_idx < packages.size(); batch_idx++) {

	std::vector<BaseObject> proposals;
	std::vector<BaseObject> objects;

    unistream::UNIFrameInfoPtr package = packages[batch_idx];
    const auto frame = package->collection.Get<unistream::UNIDataFramePtr>(unistream::kUNIDataFrameTag);
    unistream::UNIInferObjsPtr objs_holder = nullptr;
    if (package->collection.HasValue(unistream::kUNIInferObjsTag)) {
      objs_holder = package->collection.Get<unistream::UNIInferObjsPtr>(unistream::kUNIInferObjsTag);
    }

    if (!objs_holder) {
      return -1;
    }

	for (size_t i = 0; i < net_outputs.size(); i++)
	{
	  auto out_shape = model_info.OutputShape(i);
		
	  float *data = static_cast<float*>(net_outputs[i].first->GetHostData(0, batch_idx));
	  generate_proposals_yolov8_native(model_input_w / out_shape[2], data, threshold_, proposals, 
			                             model_input_w, model_input_h, NUM_CLASS);
	}

	get_out_bbox(proposals, objects, 0.5, model_input_h, model_input_w, frame->buf_surf->GetHeight(), frame->buf_surf->GetWidth());

	int box_num = objects.size();
	for (int bi = 0; bi < box_num; ++bi) {
      uint32_t id = (uint32_t)objects[bi].label;
      auto obj = std::make_shared<unistream::UNIInferObject>();
      obj->score = objects[bi].prob;
      obj->id = std::to_string(id);
      obj->bbox.x = objects[bi].rect.x / frame->buf_surf->GetWidth();
      obj->bbox.y = objects[bi].rect.y / frame->buf_surf->GetHeight();
      obj->bbox.w = objects[bi].rect.width / frame->buf_surf->GetWidth();
      obj->bbox.h = objects[bi].rect.height / frame->buf_surf->GetHeight();
      
      #if 0
      printf("out objects %lf %lf %lf %lf %u\n", 
			   obj->bbox.x, obj->bbox.y, 
			   obj->bbox.x + obj->bbox.w, obj->bbox.y + obj->bbox.h, id);
      #endif
     
      obj->AddExtraAttribute("Category", CLASS_NAMES[id]);
      objs_holder->objs_.push_back(obj);
    }
  }
  return 0;
}

int Postprocessor::ExecuteResnet50(const unistream::NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
                                   const std::vector<unistream::UNIFrameInfoPtr>& packages,
                                   const unistream::LabelStrings& labels) {
  LOGF_IF(SIMPLE_PIPELINE, net_outputs.size() != 1) << "[Postprocessor] model output size is not valid";
  LOGF_IF(SIMPLE_PIPELINE, model_info.OutputNum() != 1)
      << "[Postprocessor] model output number is not valid";

  uniedk::BufSurfWrapperPtr output = net_outputs[0].first;  // data
  if (!output->GetHostData(0)) {
    LOGE(SIMPLE_PIPELINE) << "[Postprocessor] copy data to host first.";
    return -1;
  }
  UniedkBufSurfaceSyncForCpu(output->GetBufSurface(), -1, -1);

  auto len = model_info.OutputShape(0).DataCount();

  for (size_t batch_idx = 0; batch_idx < packages.size(); batch_idx++) {
    float* data = static_cast<float*>(output->GetHostData(0, batch_idx));
    auto score_ptr = data;

    float max_score = 0;
    uint32_t label = 0;
    for (decltype(len) i = 0; i < len; ++i) {
      auto score = *(score_ptr + i);
      if (score > max_score) {
        max_score = score;
        label = i;
      }
    }
    if (threshold_ > 0 && max_score < threshold_) continue;

    unistream::UNIFrameInfoPtr package = packages[batch_idx];
    unistream::UNIInferObjsPtr objs_holder = nullptr;
    if (package->collection.HasValue(unistream::kUNIInferObjsTag)) {
      objs_holder = package->collection.Get<unistream::UNIInferObjsPtr>(unistream::kUNIInferObjsTag);
    }

    if (!objs_holder) {
      LOGE(SIMPLE_PIPELINE) << "[Postprocessor] object holder is nullptr.";
      return -1;
    }

    auto obj = std::make_shared<unistream::UNIInferObject>();
    obj->id = std::to_string(label);
    obj->score = max_score;

    if (!labels.empty() && label < labels[0].size()) {
      obj->AddExtraAttribute("Category", labels[0][label]);
    }

    std::lock_guard<std::mutex> lk(objs_holder->mutex_);
    objs_holder->objs_.push_back(obj);
  }  // for(batch_idx)

  return 0;
}

inline
void ImageSaver::OnFrame(const UNIFrameInfoSptr& frame_info) {
  const std::string output_file_name =
      FLAGS_output_dir + "/output_" + stream_id_ + "_" + std::to_string(frame_index_++) + ".jpg";
  auto frame = frame_info->collection.Get<unistream::UNIDataFramePtr>(unistream::kUNIDataFrameTag);
  cv::imwrite(output_file_name, frame->ImageBGR());
}

inline
void VideoSaver::OnStart() {
  LOGF_IF(SIMPLE_PIPELINE, !writer_.open(FLAGS_output_dir + "/output_" + stream_id_ + ".avi",
      CV_FOURCC('M', 'J', 'P', 'G'), fr_, video_size_)) << "Open video writer failed.";
}

inline
void VideoSaver::OnFrame(const UNIFrameInfoSptr& frame_info) {
  auto frame = frame_info->collection.Get<unistream::UNIDataFramePtr>(unistream::kUNIDataFrameTag);
  cv::Mat origin_img = frame->ImageBGR();
  cv::Mat resized_img;
  if (origin_img.size() != video_size_) {
    cv::resize(origin_img, resized_img, video_size_);
  } else {
    resized_img = origin_img;
  }
  writer_.write(resized_img);
}

inline
void VideoSaver::OnStop() {
  writer_.release();
}

inline
void OpencvDisplayer::OnStart() {
  last_show_time_ = std::chrono::steady_clock::now();
}

inline
void OpencvDisplayer::OnFrame(const UNIFrameInfoSptr& frame_info) {
  std::chrono::duration<double, std::milli> dura = std::chrono::steady_clock::now() - last_show_time_;
  double sleep_time = dura.count() - 1e3 / fr_;
  if (sleep_time > 0) usleep(sleep_time * 1e3);
  auto frame = frame_info->collection.Get<unistream::UNIDataFramePtr>(unistream::kUNIDataFrameTag);
  cv::imshow("simple pipeline " + stream_id_, frame->ImageBGR());
  last_show_time_ = std::chrono::steady_clock::now();
  cv::waitKey(1);
}

inline
void OpencvDisplayer::OnStop() {
  cv::destroyAllWindows();
}

SimplePipelineRunner::SimplePipelineRunner() : unistream::Pipeline("simple_pipeline") {
  // Use unistream::UNIModuleConfig to load modules statically.
  // You can also load modules by a json config file,
  // see Pipeline::BuildPipelineByJsonFile for details
  // and there are some samples used json file to config a pipeline
  // See https://www.cambricon.com/docs/unistream/user_guide_html/application/how_to_build_apps.html#id3
  // for more informations.
  // For how to implement your own module:
  // https://www.cambricon.com/docs/unistream/user_guide_html/customize_module/how_to_implement_module.html
  std::vector<unistream::UNIModuleConfig> configs;
  unistream::UNIModuleConfig decoder_config;
  decoder_config.parallelism = 0;
  decoder_config.name = "decoder";
  decoder_config.class_name = "unistream::DataSource";
  decoder_config.next = {"inferencer"};
  decoder_config.parameters = {
    std::make_pair("bufpool_size", "16"),
    std::make_pair("interval", "1"),
    std::make_pair("device_id", std::to_string(FLAGS_dev_id))
  };
  configs.push_back(decoder_config);
  unistream::UNIModuleConfig inferencer_config;
  inferencer_config.parallelism = 1;
  inferencer_config.name = "inferencer";
  inferencer_config.class_name = "unistream::Inferencer";
  inferencer_config.max_input_queue_size = 20;
  inferencer_config.next = {"osd"};
  inferencer_config.parameters = {
    std::make_pair("model_path", FLAGS_model_path),  // cambricon offline model path
    std::make_pair("preproc", "name=simple_pipeline::Preprocessor"),  // the preprocessor
    std::make_pair("postproc", "name=simple_pipeline::Postprocessor;threshold=0.5"),  // the postprocessor
    std::make_pair("batch_timeout", "300"),
    std::make_pair("engine_num", "4"),
    std::make_pair("model_input_pixel_format", "BGR24"),
    std::make_pair("device_id", std::to_string(FLAGS_dev_id))
  };
  configs.push_back(inferencer_config);
  // Osd module used to draw detection results and classification results in origin images.
  unistream::UNIModuleConfig osd_config;
  osd_config.parallelism = 1;
  osd_config.name = "osd";
  osd_config.class_name = "unistream::Osd";
  osd_config.max_input_queue_size = 20;
  osd_config.parameters = {
    std::make_pair("label_path", FLAGS_label_path)
  };
  configs.push_back(osd_config);
  LOGF_IF(SIMPLE_PIPELINE, !BuildPipeline(configs)) << "Build pipeline failed.";

  // Gets source module, then we can add data into pipeline in SimplePipelineRunner::Start.
  source_ = dynamic_cast<unistream::DataSource*>(GetModule("decoder"));
  // This show you one way to get pipeline results.
  // Set a module observer for the last module named 'osd',
  // then we can get every data from `notifier` function which is overwritted from unistream::IModuleObserver.
  // Another more recommended way is to implement your own module and place it to the last of the pipeline,
  // then you can get every data in the module implemented by yourself.
  GetModule("osd")->SetObserver(this);

  // Set a stream message observer, then we can get message from pipeline, and we especially need to pay attention to
  // EOS message which tells us the input stream is ended.
  SetStreamMsgObserver(this);
}

int SimplePipelineRunner::StartPipeline() {
  if (!Start()) return -1;
  return 0;
}

int SimplePipelineRunner::AddStream(const std::string& url, const std::string& stream_id, VisualizerBase* visualizer) {
  std::unique_lock<std::mutex> lk(mutex_);
  if (url.size() > 4 && "rtsp" == url.substr(0, 4)) {
    unistream::RtspSourceParam param;
    param.url_name = url;
    param.use_ffmpeg = false;
    param.reconnect = 10;
    if (source_ && !source_->AddSource(unistream::CreateSource(source_, stream_id, param))) {
      IncreaseStream(stream_id);
      visualizer_map_[stream_id] = visualizer;
      if (visualizer) {
        visualizer->OnStart();
      }
      return 0;
    }
  } else {
    unistream::FileSourceParam param;
    param.filename = url;
    param.framerate = -1;
    param.loop = false;
    if (source_ && !source_->AddSource(unistream::CreateSource(source_, stream_id, param))) {
      IncreaseStream(stream_id);
      visualizer_map_[stream_id] = visualizer;
      if (visualizer) {
        visualizer->OnStart();
      }
      return 0;
    }
  }
  return -1;
}

int SimplePipelineRunner::RemoveStream(const std::string& stream_id) {
  if (source_ && !source_->RemoveSource(stream_id)) {
    return 0;
  }
  return -1;
}

void SimplePipelineRunner::WaitPipelineDone() {
  while (1) {
    std::unique_lock<std::mutex> lk(mutex_);
    if (force_exit_) break;
    if (stream_set_.empty()) {
      stop_ = true;
      force_exit_ = true;  // exit when all streams done
    }
    wakener_.wait_for(lk, std::chrono::milliseconds(100), [this]() {
      return stop_.load() || force_exit_.load();
    });
    lk.unlock();
  }
  LOGI(SIMPLE_PIPELINE) << "WaitForStop(): before pipeline Stop";
  if (!stop_.load()) {
    std::unique_lock<std::mutex> lk(mutex_);
    if (nullptr != source_) {
      source_->RemoveSources();
    }
    wakener_.wait_for(lk, std::chrono::seconds(10), [this]() { return stop_.load(); });
  }
  this->Stop();
  UniedkPlatformUninit();
  source_ = nullptr;
  LOGI(SIMPLE_PIPELINE) << "WaitForStop(): pipeline Stop";
}

inline
void SimplePipelineRunner::Notify(UNIFrameInfoSptr frame_info) {
  // eos frame has no data needed to be processed.
  if (!frame_info->IsEos()) {
    std::unique_lock<std::mutex> lk(mutex_);
    VisualizerBase* visualizer = nullptr;
    if (visualizer_map_.find(frame_info->stream_id) != visualizer_map_.end()) {
      visualizer = visualizer_map_[frame_info->stream_id];
    }
    lk.unlock();
    if (visualizer) {
      visualizer->OnFrame(frame_info);
    }
  }
}

void SimplePipelineRunner::Update(const unistream::StreamMsg& msg) {
  std::lock_guard<std::mutex> lg(mutex_);
  switch (msg.type) {
    case unistream::StreamMsgType::EOS_MSG:
      LOGI(SIMPLE_PIPELINE) << "[" << this->GetName() << "] End of stream [" << msg.stream_id << "].";
      if (stream_set_.find(msg.stream_id) != stream_set_.end()) {
        if (source_) source_->RemoveSource(msg.stream_id);
        if (visualizer_map_[msg.stream_id]) visualizer_map_[msg.stream_id]->OnStop();
        stream_set_.erase(msg.stream_id);
      }
      if (stream_set_.empty()) {
        LOGI(SIMPLE_PIPELINE) << "[" << this->GetName() << "] received all EOS";
        stop_ = true;
      }
      break;
    case unistream::StreamMsgType::FRAME_ERR_MSG:
      LOGW(SIMPLE_PIPELINE) << "Frame error, pts [" << msg.pts << "].";
      break;
    default:
      LOGF(SIMPLE_PIPELINE) << "Something wrong happend, msg type [" << static_cast<int>(msg.type) << "].";
  }
  if (stop_) {
    wakener_.notify_one();
  }
}

}  // namespace simple_pipeline

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);
  FLAGS_stderrthreshold = google::INFO;
  FLAGS_colorlogtostderr = true;

  if (!simple_pipeline::InitGlobalValues()) return 1;

  UniedkPlatformConfig config;
  memset(&config, 0, sizeof(config));
  if (FLAGS_codec_id_start) {
    config.codec_id_start = FLAGS_codec_id_start;
  }
  if (UniedkPlatformInit(&config) < 0) {
    LOGE(SIMPLE_PIPELINE) << "Init platform failed";
    return -1;
  }

  std::vector<std::unique_ptr<simple_pipeline::VisualizerBase>> visualizer_vec;
  std::vector<std::string> stream_id_vec;
  for (int i = 0; i < FLAGS_input_num; i++) {
    stream_id_vec.push_back("stream_" + std::to_string(i));
    std::unique_ptr<simple_pipeline::VisualizerBase> visualizer = nullptr;
    if ("image" == FLAGS_how_to_show) {
      visualizer.reset(new simple_pipeline::ImageSaver(stream_id_vec[i]));
    } else if ("video" == FLAGS_how_to_show) {
      visualizer.reset(new simple_pipeline::VideoSaver(FLAGS_output_frame_rate, stream_id_vec[i]));
    // } else if ("display" == FLAGS_how_to_show) {
    //   visualizer.reset(new simple_pipeline::OpencvDisplayer(FLAGS_output_frame_rate, stream_id_vec[i]));
    } else {
      LOGW(SIMPLE_PIPELINE) << "Result will not show. Set flag [how_to_show] to [image/video] if show";
    }
    visualizer_vec.push_back(std::move(visualizer));
  }

  simple_pipeline::SimplePipelineRunner runner;
  if (runner.StartPipeline() != 0) {
    LOGE(SIMPLE_PIPELINE) << "Start pipeline failed.";
    return 1;
  }

  for (int i = 0; i < FLAGS_input_num; i++) {
    if (runner.AddStream(FLAGS_input_url, stream_id_vec[i], visualizer_vec[i].get()) != 0) {
      LOGE(SIMPLE_PIPELINE) << "Add stream failed.";
      return 1;
    }
  }

  LOGI(SIMPLE_PIPELINE) << "Running...";
  runner.WaitPipelineDone();

  google::ShutdownGoogleLogging();
  return 0;
}
