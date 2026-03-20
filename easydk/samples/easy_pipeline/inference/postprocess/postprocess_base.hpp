/*
 * AXERA is pleased to support the open source community by making ax-samples available.
 * 
 * Copyright (c) 2024, Axera Semiconductor Co., Ltd. All rights reserved.
 * 
 * Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 * 
 * https://opensource.org/licenses/BSD-3-Clause
 * 
 * Unless required by applicable law or agreed to in writing, software distributed
 * under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations under the License.
 */

/*
 * Copyright (C) [2025] by UNIStream Team. All rights reserved 
 *  This file has been modified by UNIStream development team based on the original work from Cambricon, Inc.
 */

/*
 * Copyright (c) 2025, UNIStream Team. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the above copyright notice and this permission notice
 * are included in all copies or substantial portions of the software.
 */

#ifndef SAMPLE_POST_PROCESS_YOLOV_BASE_HPP_
#define SAMPLE_POST_PROCESS_YOLOV_BASE_HPP_

#include <algorithm>
#include <memory>
#include <vector>
#include <cfloat>
#include <cmath>

#include "glog/logging.h"
#include "edk_frame.hpp"
#include "postprocess_base.hpp"

#define CLIP(x) ((x) < 0 ? 0 : ((x) > 1 ? 1 : (x)))

typedef struct BaseObject
{
    cv::Rect_<float> rect;
    int label;
    float prob;
    cv::Point2f landmark[5];
    std::vector<float> mask_feat;
    std::vector<float> kps_feat;
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

static void generate_proposals_yolov7_face(int stride, const float* feat, float prob_threshold, std::vector<BaseObject>& objects,
										   int letterbox_cols, int letterbox_rows, const float* anchors, float prob_threshold_unsigmoid)
{
	int anchor_num = 3;
	int feat_w = letterbox_cols / stride;
	int feat_h = letterbox_rows / stride;
	int cls_num = 1;
	int anchor_group;
	if (stride == 8)
		anchor_group = 1;
	if (stride == 16)
		anchor_group = 2;
	if (stride == 32)
		anchor_group = 3;

	auto feature_ptr = feat;

	for (int h = 0; h <= feat_h - 1; h++)
	{
		for (int w = 0; w <= feat_w - 1; w++)
		{
			for (int a = 0; a <= anchor_num - 1; a++)
			{
				if (feature_ptr[4] < prob_threshold_unsigmoid)
				{
					feature_ptr += (cls_num + 5 + 15);
					continue;
				}

				//process cls score
				int class_index = 0;
				float class_score = -FLT_MAX;
				for (int s = 0; s <= cls_num - 1; s++)
				{
					float score = feature_ptr[s + 5 + 15];
					if (score > class_score)
					{
						class_index = s;
						class_score = score;
					}
				}
				//process box score
				float box_score = feature_ptr[4];
				float final_score = sigmoid(box_score) * sigmoid(class_score);

				if (final_score >= prob_threshold)
				{
					float dx = sigmoid(feature_ptr[0]);
					float dy = sigmoid(feature_ptr[1]);
					float dw = sigmoid(feature_ptr[2]);
					float dh = sigmoid(feature_ptr[3]);
					float pred_cx = (dx * 2.0f - 0.5f + w) * stride;
					float pred_cy = (dy * 2.0f - 0.5f + h) * stride;
					float anchor_w = anchors[(anchor_group - 1) * 6 + a * 2 + 0];
					float anchor_h = anchors[(anchor_group - 1) * 6 + a * 2 + 1];
					float pred_w = dw * dw * 4.0f * anchor_w;
					float pred_h = dh * dh * 4.0f * anchor_h;
					float x0 = pred_cx - pred_w * 0.5f;
					float y0 = pred_cy - pred_h * 0.5f;
					float x1 = pred_cx + pred_w * 0.5f;
					float y1 = pred_cy + pred_h * 0.5f;

					BaseObject obj;
					obj.rect.x = x0;
					obj.rect.y = y0;
					obj.rect.width = x1 - x0;
					obj.rect.height = y1 - y0;
					obj.label = class_index;
					obj.prob = final_score;

					const float* landmark_ptr = feature_ptr + 6;
					for (int l = 0; l < 5; l++)
					{
						float lx = (landmark_ptr[3 * l] * 2.0f - 0.5f + w) * stride;
						float ly = (landmark_ptr[3 * l + 1] * 2.0f - 0.5f + h) * stride;
						//float score = sigmoid(landmark_ptr[3 * l + 2]);
						obj.landmark[l] = cv::Point2f(lx, ly);
					}

					objects.push_back(obj);
				}

				feature_ptr += (cls_num + 5 + 15);
			}
		}
	}
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
		#if 0
		printf("src objects %4.0f %4.0f %4.0f %4.0f\n", 
			   objects[i].rect.x, objects[i].rect.y, 
			   objects[i].rect.x + objects[i].rect.width, objects[i].rect.y + objects[i].rect.height);
		#endif
		
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

#endif

