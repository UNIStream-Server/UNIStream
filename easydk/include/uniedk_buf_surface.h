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

/**
 * @file uniedk_buf_surface.h
 * <b>UniedkBufSurface Interface </b>
 *
 * This file specifies the UniedkBufSurface management API.
 *
 * The UniedkBufSurface API provides methods to allocate / deallocate and copy batched buffers.
 */

#ifndef UNIEDK_BUF_SURFACE_H_
#define UNIEDK_BUF_SURFACE_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Defines the default padding length for reserved fields of structures. */
#define UNIEDK_PADDING_LENGTH  4

/** Defines the maximum number of planes. */
#define UNIEDK_BUF_MAX_PLANES   3

/**
 * Specifies color formats for \ref UniedkBufSurface.
 */
typedef enum {
  /** Specifies an invalid color format. */
  UNIEDK_BUF_COLOR_FORMAT_INVALID,
  /** Specifies 8 bit GRAY scale - single plane */
  UNIEDK_BUF_COLOR_FORMAT_GRAY8,
  /** Specifies BT.601 colorspace - YUV420 multi-planar. */
  UNIEDK_BUF_COLOR_FORMAT_YUV420,
  /** Specifies BT.601 colorspace - Y/CbCr 4:2:0 multi-planar. */
  UNIEDK_BUF_COLOR_FORMAT_NV12,
  /** Specifies BT.601 colorspace - Y/CbCr 4:2:0 multi-planar. */
  UNIEDK_BUF_COLOR_FORMAT_NV21,
  /** Specifies ARGB-8-8-8-8 single plane. */
  UNIEDK_BUF_COLOR_FORMAT_ARGB,
  /** Specifies ABGR-8-8-8-8 single plane. */
  UNIEDK_BUF_COLOR_FORMAT_ABGR,
  /** Specifies RGB-8-8-8 single plane. */
  UNIEDK_BUF_COLOR_FORMAT_RGB,
  /** Specifies BGR-8-8-8 single plane. */
  UNIEDK_BUF_COLOR_FORMAT_BGR,
  /** Specifies BGRA-8-8-8-8 single plane. */
  UNIEDK_BUF_COLOR_FORMAT_BGRA,
  /** Specifies RGBA-8-8-8-8 single plane. */
  UNIEDK_BUF_COLOR_FORMAT_RGBA,
  /*TODO(gaoyujia): add more color format
  */
  /** Specifies ARGB-1-5-5-5 single plane. */
  UNIEDK_BUF_COLOR_FORMAT_ARGB1555,

  /** for inference*/
  UNIEDK_BUF_COLOR_FORMAT_TENSOR,
  UNIEDK_BUF_COLOR_FORMAT_LAST,
} UniedkBufSurfaceColorFormat;

/**
 * Specifies memory types for \ref UniedkBufSurface.
 */
typedef enum {
  /** Specifies the default memory type, i.e. \ref UNIEDK_BUF_MEM_DEVICE
   for MLUxxx, \ref UNIEDK_BUF_MEM_UNIFIED for CExxxx. Use \ref UNIEDK_BUF_MEM_DEFAULT
   to allocate whichever type of memory is appropriate for the platform. */
  UNIEDK_BUF_MEM_DEFAULT,
  /** Specifies MLU Device memory type. valid only for MLUxxx*/
  UNIEDK_BUF_MEM_DEVICE,
  /** Specifies Host memory type. valid only for MLUxxx */
  UNIEDK_BUF_MEM_PINNED,
  /** Specifies Unified memory type. Valid only for CExxxx. */
  UNIEDK_BUF_MEM_UNIFIED,
  UNIEDK_BUF_MEM_UNIFIED_CACHED,
  /** Specifies VB memory type. Valid only for CExxxx. */
  UNIEDK_BUF_MEM_VB,
  UNIEDK_BUF_MEM_VB_CACHED,
  /** Specifies memory allocated by malloc(). */
  UNIEDK_BUF_MEM_SYSTEM,
} UniedkBufSurfaceMemType;

/**
 * Holds the planewise parameters of a buffer.
 */
typedef struct UniedkBufSurfacePlaneParams {
  /** Holds the number of planes. */
  uint32_t num_planes;
  /** Holds the widths of planes. */
  uint32_t width[UNIEDK_BUF_MAX_PLANES];
  /** Holds the heights of planes. */
  uint32_t height[UNIEDK_BUF_MAX_PLANES];
  /** Holds the pitches of planes in bytes. */
  uint32_t pitch[UNIEDK_BUF_MAX_PLANES];
  /** Holds the offsets of planes in bytes. */
  uint32_t offset[UNIEDK_BUF_MAX_PLANES];
  /** Holds the sizes of planes in bytes. */
  uint32_t psize[UNIEDK_BUF_MAX_PLANES];
  /** Holds the number of bytes occupied by a pixel in each plane. */
  uint32_t bytes_per_pix[UNIEDK_BUF_MAX_PLANES];

  void * _reserved[UNIEDK_PADDING_LENGTH * UNIEDK_BUF_MAX_PLANES];
} UniedkBufSurfacePlaneParams;

/**
 * Holds parameters required to allocate an \ref UniedkBufSurface.
 */
typedef struct UniedkBufSurfaceCreateParams {
  /** Holds the type of memory to be allocated. Not valid for UNIEDK_BUF_MEM_VB* */
  UniedkBufSurfaceMemType mem_type;

  /** Holds the Device ID. */
  uint32_t device_id;
  /** Holds the width of the buffer. */
  uint32_t width;
  /** Holds the height of the buffer. */
  uint32_t height;
  /** Holds the color format of the buffer. */
  UniedkBufSurfaceColorFormat color_format;

  /** Holds the amount of memory to be allocated. Optional; if set, all other
   parameters (width, height, etc.) are ignored. */
  uint32_t size;

  /** Holds the batch size. */
  uint32_t batch_size;

  /** Holds the alignment mode, if set,  1 bytes alignment will be applied;
   Not valid for UNIEDK_BUF_MEM_VB and UNIEDK_BUF_MEM_VB_CACHED.
  */
  bool force_align_1;

  void *_reserved[UNIEDK_PADDING_LENGTH];
} UniedkBufSurfaceCreateParams;

/**
 * Holds information about a single buffer in a batch.
 */
typedef struct UniedkBufSurfaceParams {
  /** Holds the width of the buffer. */
  uint32_t width;
  /** Holds the height of the buffer. */
  uint32_t height;
  /** Holds the pitch of the buffer. */
  uint32_t pitch;
  /** Holds the color format of the buffer. */
  UniedkBufSurfaceColorFormat color_format;

  /** Holds the amount of allocated memory. */
  uint32_t data_size;

  /** Holds a pointer to allocated memory. */
  void * data_ptr;

  /** Holds a pointer to allocated memory. */
  void * vir_addr;

  /** Holds a pointer to a CPU mapped buffer.
  Valid only for UNIEDK_BUF_MEM_UNIFIED* and UNIEDK_BUF_MEM_VB* */
  void * mapped_data_ptr;

  /** Holds planewise information (width, height, pitch, offset, etc.). */
  UniedkBufSurfacePlaneParams plane_params;

  void * _reserved[UNIEDK_PADDING_LENGTH];
} UniedkBufSurfaceParams;

/**
 * Holds information about batched buffers.
 */
typedef struct UniedkBufSurface {
  /** Holds type of memory for buffers in the batch. */
  UniedkBufSurfaceMemType mem_type;

  /** Holds a Device ID. */
  uint32_t device_id;
  /** Holds the batch size. */
  uint32_t batch_size;
  /** Holds the number valid and filled buffers. Initialized to zero when
   an instance of the structure is created. */
  uint32_t num_filled;

  /** Holds an "is contiguous" flag. If set, memory allocated for the batch
  is contiguous. Not valid for UNIEDK_BUF_MEM_VB on CE3226 */
  bool is_contiguous;

  /** Holds a pointer to an array of batched buffers. */
  UniedkBufSurfaceParams *surface_list;

  /** Holds a pointer to the buffer pool context */
  void *opaque;

  /** Holds the timestamp for video image, valid only for batch_size == 1 */
  uint64_t pts;

  void * _reserved[UNIEDK_PADDING_LENGTH];
} UniedkBufSurface;

/**
 * @brief  Creates a Buffer Pool.
 *
 * Call UniedkBufPoolDestroy() to free resources allocated by this function.
 *
 * @param[out] pool         An indirect pointer to the buffer pool.
 * @param[in]  params       A pointer to an \ref UniedkBufSurfaceCreateParams
 *                           structure.
 * @param[in]  block_num    The block number.
 *
 * @return Returns 0 if this function has run successfully. Otherwise returns -1.
 */
int UniedkBufPoolCreate(void **pool, UniedkBufSurfaceCreateParams *params, uint32_t block_num);

/**
 * @brief  Frees the buffer pool previously allocated by UniedkBufPoolCreate().
 *
 * @param[in] surf  A pointer to an \ref buffer pool to be freed.
 *
 * @return Returns 0 if this function has run successfully. Otherwise returns -1.
 */
int UniedkBufPoolDestroy(void *pool);

/**
 * @brief  Allocates a single buffer.
 *
 * Allocates memory for a buffer and returns a pointer to an
 * allocated \ref UniedkBufSurface. The \a params structure must have
 * the allocation parameters of a single buffer.
 *
 * Call UniedkBufSurfaceDestroy() to free resources allocated by this function.
 *
 * @param[out] surf         An indirect pointer to the allocated buffer.
 * @param[in]  pool         A pointer to a buffer pool.
 *
 * @return Returns 0 if this function has run successfully. Otherwise returns -1.
 */
int UniedkBufSurfaceCreateFromPool(UniedkBufSurface **surf, void *pool);

/**
 * @brief  Allocates a batch of buffers.
 *
 * Allocates memory for \a batch_size buffers and returns a pointer to an
 * allocated \ref UniedkBufSurface. The \a params structure must have
 * the allocation parameters. If \a params.size
 * is set, a buffer of that size is allocated, and all other
 * parameters (width, height, color format, etc.) are ignored.
 *
 * Call UniedkBufSurfaceDestroy() to free resources allocated by this function.
 *
 * @param[out] surf         An indirect pointer to the allocated batched
 *                           buffers.
 * @param[in]  params       A pointer to an \ref UniedkBufSurfaceCreateParams
 *                           structure.
 *
 * @return Returns 0 if this function has run successfully. Otherwise returns -1.
 */
int UniedkBufSurfaceCreate(UniedkBufSurface **surf, UniedkBufSurfaceCreateParams *params);

/**
 * @brief  Frees a single buffer allocated by UniedkBufSurfaceCreate()
 *         or batched buffers previously allocated by UniedkBufSurfaceCreate().
 *
 * @param[in] surf  A pointer to an \ref BufSurface to be freed.
 *
 * @return Returns 0 if this function has run successfully. Otherwise returns -1.
 */
int UniedkBufSurfaceDestroy(UniedkBufSurface *surf);

/**
 * @brief  Syncs the hardware memory cache for the CPU.
 *
 * Valid only for memory types \ref UNIEDK_BUF_MEM_UNIFIED_CACHED and
 * \ref UNIEDK_BUF_MEM_VB_CACHED.
 *
 * @param[in] surf      A pointer to an \ref NvBufSurface structure.
 * @param[in] index     Index of the buffer in the batch. -1 refers to
 *                      all buffers in the batch.
 * @param[in] plane     Index of a plane in the buffer. -1 refers to all planes
 *                      in the buffer.
 *
 * @return Returns 0 if this function has run successfully. Otherwise returns -1.
 */
int UniedkBufSurfaceSyncForCpu(UniedkBufSurface *surf, int index, int plane);

/**
 * @brief  Syncs the hardware memory cache for the device.
 *
 * Valid only for memory types \ref UNIEDK_BUF_MEM_UNIFIED and
 * \ref UNIEDK_BUF_MEM_VB.
 *
 * @param[in] surf      A pointer to an \ref UniedkBufSurface structure.
 * @param[in] index     Index of a buffer in the batch. -1 refers to all buffers
 *                      in the batch.
 * @param[in] plane     Index of a plane in the buffer. -1 refers to all planes
 *                      in the buffer.
 *
 * @return Returns 0 if this function has run successfully. Otherwise returns -1.
 */
int UniedkBufSurfaceSyncForDevice(UniedkBufSurface *surf, int index, int plane);

/**
 * @brief  Copies the content of source batched buffer(s) to destination
 * batched buffer(s).
 *
 * The source and destination \ref UniedkBufSurface objects must have same
 * buffer and batch size.
 *
 * @param[in] src_surf   A pointer to the source UniedkBufSurface structure.
 * @param[out] dst_surf   A pointer to the destination UniedkBufSurface structure.
 *
 * @return Returns 0 if this function has run successfully. Otherwise returns -1.
 */
int UniedkBufSurfaceCopy(UniedkBufSurface *src_surf, UniedkBufSurface *dst_surf);

/**
 * @brief  Fills each byte of the buffer(s) in an \ref UniedkBufSurface with a
 * provided value.
 *
 * You can also use this function to reset the buffer(s) in the batch.
 *
 * @param[in] surf  A pointer to the UniedkBufSurface structure.
 * @param[in] index Index of a buffer in the batch. -1 refers to all buffers
 *                  in the batch.
 * @param[in] plane Index of a plane in the buffer. -1 refers to all planes
 *                  in the buffer.
 * @param[in] value The value to be used as fill.
 *
 * @return Returns 0 if this function has run successfully. Otherwise returns -1.
 */
int UniedkBufSurfaceMemSet(UniedkBufSurface *surf, int index, int plane, uint8_t value);

/** @} */

#ifdef __cplusplus
}
#endif

#endif  // UNIEDK_BUF_SURFACE_H_
