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

#ifndef UNIEDK_TRANSFORM_H_
#define UNIEDK_TRANSFORM_H_

#include <stdint.h>
#include <stdbool.h>
#include "uniedk_buf_surface.h"
#include "axcl_rt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UNIEDK_TRANSFORM_MAX_CHNS   4
/**
 * Specifies compute devices used by UniedkTransform.
 */
typedef enum {
  /** Specifies VGU as a compute device for CExxxx or MLU for MLUxxx. */
  UNIEDK_TRANSFORM_COMPUTE_DEFAULT,
  /** Specifies that the MLU is the compute device. */
  UNIEDK_TRANSFORM_COMPUTE_MLU,
  /** Specifies that the VGU as a compute device. Only supported on CExxxx. */
  UNIEDK_TRANSFORM_COMPUTE_VGU,
  /** Specifies the number of compute modes. */
  UNIEDK_TRANSFORM_COMPUTE_NUM
} UniedkTransformComputeMode;

/**
 * Specifies transform types.
 */
typedef enum {
  /** Specifies a transform to crop the source rectangle. */
  UNIEDK_TRANSFORM_CROP_SRC   = 1,
  /** Specifies a transform to crop the destination rectangle. */
  UNIEDK_TRANSFORM_CROP_DST   = 1 << 1,
  /** Specifies a transform to set the filter type. */
  UNIEDK_TRANSFORM_FILTER     = 1 << 2,
  /** Specifies a transform to normalize output. */
  UNIEDK_TRANSFORM_MEAN_STD  = 1 << 3
} UniedkTransformFlag;

/**
 * Holds the coordinates of a rectangle.
 */
typedef struct UniedkTransformRect {
  /** Holds the rectangle top. */
  uint32_t top;
  /** Holds the rectangle left side. */
  uint32_t left;
  /** Holds the rectangle width. */
  uint32_t width;
  /** Holds the rectangle height. */
  uint32_t height;
} UniedkTransformRect;

/**
 * Specifies data type.
 */
typedef enum {
  /** Specifies the data type to uint8. */
  UNIEDK_TRANSFORM_UINT8,
  /** Specifies the data type to float32. */
  UNIEDK_TRANSFORM_FLOAT32,
  /** Specifies the data type to float16. */
  UNIEDK_TRANSFORM_FLOAT16,
  /** Specifies the data type to int16. */
  UNIEDK_TRANSFORM_INT16,
  /** Specifies the data type to int32. */
  UNIEDK_TRANSFORM_INT32,
  /** Specifies the number of data types. */
  UNIEDK_TRANSFORM_NUM
} UniedkTransformDataType;

/**
 * Specifies color format.
 */
typedef enum {
  /** Specifies ABGR-8-8-8-8 single plane. */
  UNIEDK_TRANSFORM_COLOR_FORMAT_ARGB,
  /** Specifies ABGR-8-8-8-8 single plane. */
  UNIEDK_TRANSFORM_COLOR_FORMAT_ABGR,
  /** Specifies BGRA-8-8-8-8 single plane. */
  UNIEDK_TRANSFORM_COLOR_FORMAT_BGRA,
  /** Specifies RGBA-8-8-8-8 single plane. */
  UNIEDK_TRANSFORM_COLOR_FORMAT_RGBA,
  /** Specifies RGB-8-8-8 single plane. */
  UNIEDK_TRANSFORM_COLOR_FORMAT_RGB,
  /** Specifies BGR-8-8-8 single plane. */
  UNIEDK_TRANSFORM_COLOR_FORMAT_BGR,
  /** Specifies the number of color formats. */
  UNIEDK_TRANSFORM_COLOR_FORMAT_NUM,
} UniedkTransformColorFormat;

/**
 * Holds the shape information.
 */
typedef struct UniedkTransformShape {
  /** Holds the dimension n. Normally represents batch size */
  uint32_t n;
  /** Holds the dimension c. Normally represents channel */
  uint32_t c;
  /** Holds the dimension h. Normally represents height */
  uint32_t h;
  /** Holds the dimension h. Normally represents width */
  uint32_t w;
} UniedkTransformShape;

/**
 * Holds the descriptions of a tensor.
 */
typedef struct UniedkTransformTensorDesc {
  /** Holds the shape of the tensor */
  UniedkTransformShape shape;
  /** Holds the data type of the tensor */
  UniedkTransformDataType data_type;
  /** Holds the color format of the tensor */
  UniedkTransformColorFormat color_format;
} UniedkTransformTensorDesc;

/**
 * Holds the parameters of a MeanStd tranformation.
 */
typedef struct UniedkTransformMeanStdParams {
  /** Holds a pointer of mean values */
  float mean[UNIEDK_TRANSFORM_MAX_CHNS];
  /** Holds a pointer of std values */
  float std[UNIEDK_TRANSFORM_MAX_CHNS];
} UniedkTransformMeanStdParams;

/**
 * Holds configuration parameters for a transform/composite session.
 */
typedef struct UniedkTransformConfigParams {
  /** Holds the mode of operation:  VGU (CE3226) or MLU (M370/CE3226)
   If VGU is configured, device_id is ignored. */
  UniedkTransformComputeMode compute_mode;

  /** Holds the Device ID to be used for processing. */
  int32_t device_id;
} UniedkTransformConfigParams;

/**
 * Holds transform parameters for a transform call.
 */
typedef struct UniedkTransformParams {
  /** Holds a flag that indicates which transform parameters are valid. */
  uint32_t transform_flag;
  /** Hold a pointer of normalize value*/
  UniedkTransformMeanStdParams *mean_std_params;
  /** Hold a pointer of tensor desc of src */
  UniedkTransformTensorDesc *src_desc;

  /** Not used. Hold a pointer of tensor desc of dst */
  UniedkTransformTensorDesc *dst_desc;

  /** Holds a pointer to a list of source rectangle coordinates for
   a crop operation. */
  UniedkTransformRect *src_rect;
  /** Holds a pointer to list of destination rectangle coordinates for
   a crop operation. */
  UniedkTransformRect *dst_rect;
} UniedkTransformParams;


/**
 * @brief  Sets user-defined session parameters.
 *
 * If user-defined session parameters are set, they override the
 * UniedkTransform() function's default session.
 *
 * @param[in] config_params     A pointer to a structure that is populated
 *                              with the session parameters to be used.
 *
 * @return Returns 0 if this function run successfully, otherwise returns non-zero values.
 */
int UniedkTransformSetSessionParams(UniedkTransformConfigParams *config_params);

/**
 * @brief Gets the session parameters used by UniedkTransform().
 *
 * @param[out] config_params    A pointer to a caller-allocated structure to be
 *                              populated with the session parameters used.
 *
 * @return Returns 0 if this function run successfully, otherwise returns non-zero values.
 */
int UniedkTransformGetSessionParams(UniedkTransformConfigParams *config_params);

/**
 * @brief Performs a transformation on batched input images.
 *
 * @param[in]  src  A pointer to input batched buffers to be transformed.
 * @param[out] dst  A pointer to a caller-allocated location where
 *                  transformed output is to be stored.
 *                  @par When destination cropping is performed, memory outside
 *                  the crop location is not touched, and may contain stale
 *                  information. The caller must perform a memset before
 *                  calling this function if stale information must be
 *                  eliminated.
 * @param[in]  transform_params
 *                  A pointer to an UniedkTransformParams structure
 *                  which specifies the type of transform to be performed. They
 *                  may include any combination of scaling, format conversion,
 *                  and cropping for both source and destination.
 * @return Returns 0 if this function run successfully, otherwise returns non-zero values.
 */
int UniedkTransform(UniedkBufSurface *src, UniedkBufSurface *dst, UniedkTransformParams *transform_params);

#ifdef __cplusplus
}
#endif

#endif  // UNIEDK_TRANSFORM_H_
