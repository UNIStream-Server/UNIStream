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
#ifndef UNIEDK_OSD_H_
#define UNIEDK_OSD_H_

#include <stdint.h>
#include <stdbool.h>

#include "uniedk_buf_surface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Holds the parameters of a rectangle.
 */
typedef struct UniedkOsdRectParams {
  /** The left top coordinate x. */
  int x;
  /** The left top coordinate y. */
  int y;
  /** The width of the rectangle. */
  int w;
  /** The height of the rectangle. */
  int h;
  /** The color of the rectangle boundary lines. 0x00rrggbb */
  uint32_t color;
  /** The width of the rectangle boundary lines. Only valid for UniedkDrawRect */
  uint32_t line_width;
} UniedkOsdRectParams;

/**
 * Holds the parameters of a bitmap.
 */
typedef struct UniedkOsdBitmapParams {
  /** The left top coordinate x. */
  int x;
  /** The left top coordinate y. */
  int y;
  /** The width of the bitmap. */
  int w;
  /** The height of the bitmap. */
  int h;
  /** The pitch of the bitmap. */
  uint32_t pitch;
  /** The the bitmap */
  void *bitmap_argb1555;
  /** The bg_color of the bitmap */
  uint32_t bg_color;  // 0x00rrggbb;
} UniedkOsdBitmapParams;

/**
 * @brief Draws rectangle.
 *
 * @param[in,out] surf A pointer points to UniedkBufSurface. Draws rectangle on it.
 * @param[in] params The parameters for drawing rectangles.
 * @param[in] num The number of rectangles.
 *
 * @return Returns 0 if this function has run successfully. Otherwise returns -1.
 */
int UniedkDrawRect(UniedkBufSurface *surf, UniedkOsdRectParams *params, uint32_t num);
/**
 * @brief Fills rectangle.
 *
 * @param[in,out] surf A pointer points to UniedkBufSurface. Fills rectangle on it.
 * @param[in] params The parameters for filling rectangles.
 * @param[in] num The number of rectangles.
 *
 * @return Returns 0 if this function has run successfully. Otherwise returns -1.
 */
int UniedkFillRect(UniedkBufSurface *surf, UniedkOsdRectParams *params, uint32_t num);
/**
 * @brief Draws bitmap.
 *
 * @param[in,out] surf A pointer points to UniedkBufSurface. Draws bitmap on it.
 * @param[in] params The parameters for drawing bitmaps.
 * @param[in] num The number of bitmaps.
 *
 * @return Returns 0 if this function has run successfully. Otherwise returns -1.
 */
int UniedkDrawBitmap(UniedkBufSurface *surf, UniedkOsdBitmapParams *params, uint32_t num);

#ifdef __cplusplus
}
#endif

#endif  // UNIEDK_OSD_H_
