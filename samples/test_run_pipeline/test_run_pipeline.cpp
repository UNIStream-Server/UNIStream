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
DEFINE_string(output_dir, "./", "/your/path/to/output_dir");
DEFINE_int32(output_frame_rate, 25, "output frame rate");
DEFINE_int32(dev_id, 0, "device ordinal index");
DEFINE_int32(codec_id_start, 0, "vdec/venc first id, for CE3226 only");
DEFINE_string(config_fname, "", "pipeline config filename");

namespace test_pipeline {


// Init mean values and std. init channel order for color convert(eg. bgr to rgba)
using UNIFrameInfoSptr = std::shared_ptr<unistream::UNIFrameInfo>;
using SourceHandlerSptr = std::shared_ptr<unistream::SourceHandler>;


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
class TestPipelineRunner : public unistream::Pipeline, public unistream::StreamMsgObserver,
                             public unistream::IModuleObserver {
 public:
  TestPipelineRunner();
  int StartPipeline();
  int AddStream(const std::string& url, const std::string& stream_id, VisualizerBase* visualizer = nullptr);
  int RemoveStream(const std::string& stream_id);
  void WaitPipelineDone();
  void ForceStop() {
    std::unique_lock<std::mutex> lk(mutex_);
    force_exit_.store(true);
  }

  int Init(const std::string &config_filename) {
    if (!BuildPipelineByJSONFile(config_filename)) {
      LOGE(TEST_PIPELINE) << "Build pipeline failed.";
      return -1;
    }

    // Gets source module, then we can add data into pipeline in TestPipelineRunner::Start.
    source_ = dynamic_cast<unistream::DataSource*>(GetModule("source"));

    // This show you one way to get pipeline results.
    // Set a module observer for the last module named 'osd',
    // then we can get every data from `notifier` function which is overwritted from unistream::IModuleObserver.
    // Another more recommended way is to implement your own module and place it to the last of the pipeline,
    // then you can get every data in the module implemented by yourself.
    //GetModule("encoder")->SetObserver(this);
    
    return 0;
  }

 private:
  void Notify(UNIFrameInfoSptr frame_info) override;
  void Update(const unistream::StreamMsg& msg) override;

 private:
  void IncreaseStream(std::string stream_id) {
    if (stream_set_.find(stream_id) != stream_set_.end()) {
      LOGF(TEST_PIPELINE) << "IncreaseStream() The stream is ongoing []" << stream_id;
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
};  // class TestPipelineRunner

inline
void ImageSaver::OnFrame(const UNIFrameInfoSptr& frame_info) {
  const std::string output_file_name =
      FLAGS_output_dir + "/output_" + stream_id_ + "_" + std::to_string(frame_index_++) + ".jpg";
  auto frame = frame_info->collection.Get<unistream::UNIDataFramePtr>(unistream::kUNIDataFrameTag);
  cv::imwrite(output_file_name, frame->ImageBGR());
}

inline
void VideoSaver::OnStart() {
 #if 0
  LOGF_IF(TEST_PIPELINE, !writer_.open(FLAGS_output_dir + "/output_" + stream_id_ + ".avi",
      CV_FOURCC('M', 'J', 'P', 'G'), fr_, video_size_)) << "Open video writer failed.";
 #endif
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
void OpencvDisplayer::OnStop() {
  cv::destroyAllWindows();
}

TestPipelineRunner::TestPipelineRunner() : unistream::Pipeline("test_pipeline") {

  // Set a stream message observer, then we can get message from pipeline, and we especially need to pay attention to
  // EOS message which tells us the input stream is ended.
  SetStreamMsgObserver(this);
}

int TestPipelineRunner::StartPipeline() {
  if (!Start()) return -1;
  return 0;
}

int TestPipelineRunner::AddStream(const std::string& url, const std::string& stream_id, VisualizerBase* visualizer) {
  std::unique_lock<std::mutex> lk(mutex_);
  if (url.size() > 4 && "rtsp" == url.substr(0, 4)) {
    unistream::RtspSourceParam param;
    param.url_name = url;
    param.use_ffmpeg = true;
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

int TestPipelineRunner::RemoveStream(const std::string& stream_id) {
  if (source_ && !source_->RemoveSource(stream_id)) {
    return 0;
  }
  return -1;
}

void TestPipelineRunner::WaitPipelineDone() {
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
  LOGI(TEST_PIPELINE) << "WaitForStop(): before pipeline Stop";
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
  LOGI(TEST_PIPELINE) << "WaitForStop(): pipeline Stop";
}

inline
void TestPipelineRunner::Notify(UNIFrameInfoSptr frame_info) {
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

void TestPipelineRunner::Update(const unistream::StreamMsg& msg) {
  std::lock_guard<std::mutex> lg(mutex_);
  switch (msg.type) {
    case unistream::StreamMsgType::EOS_MSG:
      LOGI(TEST_PIPELINE) << "[" << this->GetName() << "] End of stream [" << msg.stream_id << "].";
      if (stream_set_.find(msg.stream_id) != stream_set_.end()) {
        if (source_) source_->RemoveSource(msg.stream_id);
        if (visualizer_map_[msg.stream_id]) visualizer_map_[msg.stream_id]->OnStop();
        stream_set_.erase(msg.stream_id);
      }
      if (stream_set_.empty()) {
        LOGI(TEST_PIPELINE) << "[" << this->GetName() << "] received all EOS";
        stop_ = true;
      }
      break;
    case unistream::StreamMsgType::FRAME_ERR_MSG:
      LOGW(TEST_PIPELINE) << "Frame error, pts [" << msg.pts << "].";
      break;
    default:
      LOGF(TEST_PIPELINE) << "Something wrong happend, msg type [" << static_cast<int>(msg.type) << "].";
  }
  if (stop_) {
    wakener_.notify_one();
  }
}

}  // namespace test_pipeline

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);
  FLAGS_stderrthreshold = google::INFO;
  FLAGS_colorlogtostderr = true;

  UniedkPlatformConfig config;
  memset(&config, 0, sizeof(config));
  if (FLAGS_codec_id_start) {
    config.codec_id_start = FLAGS_codec_id_start;
  }
  if (UniedkPlatformInit(&config) < 0) {
    LOGE(TEST_PIPELINE) << "Init platform failed";
    return -1;
  }

  std::vector<std::unique_ptr<test_pipeline::VisualizerBase>> visualizer_vec;
  std::vector<std::string> stream_id_vec;
  for (int i = 0; i < FLAGS_input_num; i++) {
    stream_id_vec.push_back("stream_" + std::to_string(i));
    std::unique_ptr<test_pipeline::VisualizerBase> visualizer = nullptr;
    if ("image" == FLAGS_how_to_show) {
      visualizer.reset(new test_pipeline::ImageSaver(stream_id_vec[i]));
    } else if ("video" == FLAGS_how_to_show) {
      visualizer.reset(new test_pipeline::VideoSaver(FLAGS_output_frame_rate, stream_id_vec[i]));
    } else {
      LOGW(TEST_PIPELINE) << "Result will not show. Set flag [how_to_show] to [image/video] if show";
    }
    visualizer_vec.push_back(std::move(visualizer));
  }

  test_pipeline::TestPipelineRunner runner;
  if (runner.Init(FLAGS_config_fname) != 0)
  {
    LOGE(TEST_PIPELINE) << "init pipeline failed.";
    return 1;
  }
  
  if (runner.StartPipeline() != 0) {
    LOGE(TEST_PIPELINE) << "Start pipeline failed.";
    return 1;
  }

  for (int i = 0; i < FLAGS_input_num; i++) {
    if (runner.AddStream(FLAGS_input_url, stream_id_vec[i], visualizer_vec[i].get()) != 0) {
      LOGE(TEST_PIPELINE) << "Add stream failed.";
      return 1;
    }
  }

  LOGI(TEST_PIPELINE) << "Running...";
  runner.WaitPipelineDone();

  google::ShutdownGoogleLogging();
  return 0;
}
