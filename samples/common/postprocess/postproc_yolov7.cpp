/*************************************************************************
 * Copyright (C) [2022] by Cambricon, Inc. All rights reserved
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
#include <vector>

#include "unistream_postproc.hpp"
#include "postproc_base.hpp"

#define CLIP(x) ((x) < 0 ? 0 : ((x) > 1 ? 1 : (x)))

class PostprocYolov7 : public unistream::Postproc {
 public:
  int Execute(const unistream::NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
              const std::vector<unistream::UNIFrameInfoPtr>& packages,
              const unistream::LabelStrings& labels) override;

 private:
  DECLARE_REFLEX_OBJECT_EX(PostprocYolov7, unistream::Postproc);
};  // class PostprocYolov7

IMPLEMENT_REFLEX_OBJECT_EX(PostprocYolov7, unistream::Postproc);

int PostprocYolov7::Execute(const unistream::NetOutputs& net_outputs, const infer_server::ModelInfo& model_info,
                            const std::vector<unistream::UNIFrameInfoPtr>& packages,
                            const unistream::LabelStrings& labels) {
  const float ANCHORS[18] = {4,5,  6,8,  10,12,              //# P3/8
                           15,19,  23,30,  39,52,          //# P4/16
                           72,97,  123,164,  209,297 };   //# P5/32

  for (size_t i = 0; i < net_outputs.size(); i++)
  {
	  if (!net_outputs[i].first->GetHostData(0)) {
	    LOGE(TEST_PIPELINE) << "[PostprocYolov7] Postprocess failed, copy data to host failed.";
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
    LOGE(TEST_PIPELINE) << "[PostprocYolov7] Postprocess failed. Unsupported dim order";
    return -1;
  }

  for (size_t batch_idx = 0; batch_idx < packages.size(); batch_idx++) {

	std::vector<BaseObject> proposals;
	std::vector<BaseObject> objects;
	float prob_threshold_u_sigmoid = -1.0f * (float)std::log((1.0f / threshold_) - 1.0f);

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

	  generate_proposals_yolov7_face(model_input_w / out_shape[2], data, threshold_, proposals, 
			                             model_input_w, model_input_h, ANCHORS, prob_threshold_u_sigmoid);
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
           
      obj->AddExtraAttribute("Category", "face");
      objs_holder->objs_.push_back(obj);
    }
  }
  return 0;
}
