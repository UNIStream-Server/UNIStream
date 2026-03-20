/*************************************************************************
 * Copyright (C) [2022] by Cambricon, Inc. All rights reserved
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

#ifndef SAMPLE_POST_PROCESS_YOLOV8_HPP_
#define SAMPLE_POST_PROCESS_YOLOV8_HPP_

#include <algorithm>
#include <memory>
#include <vector>
#include <cfloat>
#include <cmath>
#include <opencv2/opencv.hpp>

#include "uniis/infer_server.h"
#include "uniis/processor.h"

#include "glog/logging.h"
#include "edk_frame.hpp"
#include "postprocess_base.hpp"

#define CLIP(x) ((x) < 0 ? 0 : ((x) > 1 ? 1 : (x)))

class PostprocYolov8 : public infer_server::IPostproc {
 public:
  PostprocYolov8() = default;
  ~PostprocYolov8() = default;

  int OnPostproc(const std::vector<infer_server::InferData*>& data_vec,
                 const infer_server::ModelIO& model_output,
                 const infer_server::ModelInfo* model_info) override {
    int NUM_CLASS = 80;

    uniedk::BufSurfWrapperPtr output0 = model_output.surfs[0];  // data
    uniedk::BufSurfWrapperPtr output1 = model_output.surfs[1];  // bbox

	for (size_t i = 0; i < model_output.surfs.size(); i++)
	{
	    if (!model_output.surfs[i]->GetHostData(0)) {
	      LOG(ERROR) << "[EasyDK Samples] [PostprocYolov8] Postprocess failed, copy data to host failed.";
	      return -1;
	    }
	}

    infer_server::DimOrder input_order = model_info->InputLayout(0).order;
    auto s = model_info->InputShape(0);
    int model_input_w, model_input_h;
    if (input_order == infer_server::DimOrder::NCHW) {
      model_input_w = s[3];
      model_input_h = s[2];
    } else if (input_order == infer_server::DimOrder::NHWC) {
      model_input_w = s[2];
      model_input_h = s[1];
    } else {
      LOG(ERROR) << "[EasyDK Samples] [PostprocYolov8] Postprocess failed. Unsupported dim order";
      return -1;
    }

    for (size_t batch_idx = 0; batch_idx < data_vec.size(); batch_idx++) {

	  std::vector<BaseObject> proposals;
	  std::vector<BaseObject> objects;
	  for (size_t i = 0; i < model_output.surfs.size(); i++)
	  {
	    auto out_shape = model_info->OutputShape(i);
		
	    float *data = static_cast<float*>(model_output.surfs[i]->GetHostData(0, batch_idx));
		generate_proposals_yolov8_native(model_input_w / out_shape[2], data, threshold_, proposals, 
			                             model_input_w, model_input_h, NUM_CLASS);
	  }

	  get_out_bbox(proposals, objects, 0.5, model_input_h, model_input_w, model_input_h, model_input_w);

	  std::shared_ptr<EdkFrame> frame = data_vec[batch_idx]->GetUserData<std::shared_ptr<EdkFrame>>();

	  const float scaling_w = 1.0f * model_input_w / frame->surf->GetWidth();
      const float scaling_h = 1.0f * model_input_h / frame->surf->GetHeight();
      const float scaling = std::min(scaling_w, scaling_h);
      float scaling_factor_w, scaling_factor_h;
      scaling_factor_w = scaling_w / scaling;
      scaling_factor_h = scaling_h / scaling;
	  
	  int box_num = objects.size();
	  for (int bi = 0; bi < box_num; ++bi) {
	  	float l = objects[bi].rect.x / (float)model_input_w;
        float t = objects[bi].rect.y / (float)model_input_h;
        float r = l + objects[bi].rect.width / (float)model_input_w;
        float b = t + objects[bi].rect.height / (float)model_input_h;
		l = CLIP((l - 0.5f) * scaling_factor_w + 0.5f);
        t = CLIP((t - 0.5f) * scaling_factor_h + 0.5f);
        r = CLIP((r - 0.5f) * scaling_factor_w + 0.5f);
        b = CLIP((b - 0.5f) * scaling_factor_h + 0.5f);

		
        DetectObject obj;
        obj.label = objects[bi].label;
        obj.score = objects[bi].prob;
        obj.bbox.x = l;
        obj.bbox.y = t;
        obj.bbox.w = std::min(1.0f - l, r - l);
        obj.bbox.h = std::min(1.0f - t, b - t);
        frame->objs.push_back(obj);
		#if 0
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
	
		printf("out objects %4.0f %4.0f %4.0f %4.0f %s\n", 
			   obj.bbox.x, obj.bbox.y, 
			   obj.bbox.x + obj.bbox.w, obj.bbox.y + obj.bbox.h, CLASS_NAMES[obj.label]);
		#endif
      }
    }
    return 0;
  }

 private:
  float threshold_ = 0.45f;
};

#endif
