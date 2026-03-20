/*************************************************************************
 *  Copyright (C) [2020] by Cambricon, Inc. All rights reserved
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

#include "unifont.hpp"

#include <string>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif

#include "unistream_logging.hpp"

namespace unistream {

#ifdef HAVE_FREETYPE

bool UniFont::Init(const std::string& font_path, float font_pixel, float space, float step) {
  std::unique_lock<std::mutex> guard(mutex_);
  if (FT_Init_FreeType(&m_library)) {
    LOGE(OSD) << "FreeType init errors";
    return false;
  }
  if (FT_New_Face(m_library, font_path.c_str(), 0, &m_face)) {
    FT_Done_FreeType(m_library);
    LOGE(OSD) << "Can not create a font, please checkout the font path: " << m_library;
    return false;
  }
  is_initialized_ = true;

  // Set font args
  restoreFont(font_pixel, space, step);

  // Set font env
  setlocale(LC_ALL, "");

  return true;
}

UniFont::~UniFont() {
  if (is_initialized_) {
    FT_Done_Face(m_face);
    FT_Done_FreeType(m_library);
  }
}

// Restore the original font Settings
void UniFont::restoreFont(float font_pixel, float space, float step) {
  if (!is_initialized_) {
    LOGE(OSD) << " [Osd] Please init UniFont first.";
    return;
  }
  m_fontType = 0;

  m_fontSize.val[0] = font_pixel;
  m_fontSize.val[1] = space;
  m_fontSize.val[2] = step;
  m_fontSize.val[3] = 0;

  m_fontUnderline = false;

  m_fontDiaphaneity = 1.0;

  // Set character size
  FT_Set_Pixel_Sizes(m_face, static_cast<int>(m_fontSize.val[0]), 0);
}

uint32_t UniFont::GetFontPixel() {
  std::unique_lock<std::mutex> guard(mutex_);
  if (!is_initialized_) {
    LOGE(OSD) << " [Osd] Please init UniFont first.";
    return 0;
  }
  return m_fontSize.val[0];
}

int UniFont::ToWchar(char*& src, wchar_t*& dest, const char* locale) {
  if (src == NULL) {
    dest = NULL;
    return 0;
  }

  // Set the locale according to the environment variable
  setlocale(LC_CTYPE, locale);

  // Gets the required wide character size to convert to
  int w_size = mbstowcs(NULL, src, 0) + 1;

  if (w_size == 0) {
    dest = NULL;
    return -1;
  }

  dest = new (std::nothrow) wchar_t[w_size];
  if (!dest) {
    return -1;
  }

  int ret = mbstowcs(dest, src, strlen(src) + 1);
  if (ret <= 0) {
    return -1;
  }
  return 0;
}

bool UniFont::GetTextSize(char* text, uint32_t* width, uint32_t* height) {
  if (!width || !height || !text) {
    LOGE(OSD) << " [UniFont] [GetTextSize] The text, width or height is nullptr.";
    return false;
  }
  std::unique_lock<std::mutex> guard(mutex_);

  if (!is_initialized_) {
    LOGE(OSD) << " [UniFont] [GetTextSize] Please init UniFont first.";
    return false;
  }
  wchar_t* w_str;
  if (ToWchar(text, w_str) == -1) {
    LOGE(OSD) << "[UniFont] [GetTextSize] [ToWchar] failed.";
    return -1;
  }

  uint32_t w_char_width = 0, w_char_height = 0;
  double space = m_fontSize.val[0] * m_fontSize.val[1];
  double sep = m_fontSize.val[0] * m_fontSize.val[2];

  for (int i = 0; w_str[i] != '\0'; ++i) {
    GetWCharSize(w_str[i], &w_char_width, &w_char_height);
    if (*height < w_char_height) {
      *height = w_char_height;
    }
    if (w_char_width) {
      *width += w_char_width;
    } else {
      *width += space;
    }
    *width += sep;
  }
  return true;
}

void UniFont::GetWCharSize(wchar_t wc, uint32_t* width, uint32_t* height) {
  if (!width || !height) {
    LOGE(OSD) << " [UniFont] [GetWCharSize] The width or height is nullptr.";
    return;
  }

  FT_UInt glyph_index = FT_Get_Char_Index(m_face, wc);
  FT_Load_Glyph(m_face, glyph_index, FT_LOAD_DEFAULT);
  FT_Render_Glyph(m_face->glyph, FT_RENDER_MODE_MONO);

  FT_GlyphSlot slot = m_face->glyph;

  // height and width
  *height = slot->bitmap.rows;
  *width = slot->bitmap.width;
}

int UniFont::putText(UNIDataFramePtr frame, char* text, cv::Point pos, cv::Scalar color) {
  /*cv::Mat img = frame->ImageBGR();
  if (img.data == nullptr) {
    LOGE(OSD) << "[UniFont] [putText] img.data is nullptr.";
    return -1;
  }
  */
  std::unique_lock<std::mutex> guard(mutex_);
  if (text == nullptr) {
    LOGE(OSD) << "[UniFont] [putText] text is nullptr.";
  }

  if (!is_initialized_) {
    LOGE(OSD) << " [Osd] Please init UniFont first.";
    return -1;
  }

  wchar_t* w_str;
  if (ToWchar(text, w_str) == -1) {
    LOGE(OSD) << "[UniFont] [putText] [ToWchar] failed.";
    return -1;
  }

  for (int i = 0; w_str[i] != '\0'; ++i) {
    putWChar(frame, w_str[i], pos, color);
  }

  return 0;
}

// Output the current character and update the m pos position
void UniFont::putWChar(UNIDataFramePtr frame, wchar_t wc, cv::Point& pos, cv::Scalar color) {
  // Generate a binary bitmap of a font based on unicode
  FT_UInt glyph_index = FT_Get_Char_Index(m_face, wc);
  FT_Load_Glyph(m_face, glyph_index, FT_LOAD_DEFAULT);
  FT_Render_Glyph(m_face->glyph, FT_RENDER_MODE_MONO);

  FT_GlyphSlot slot = m_face->glyph;

  // Cols and rows
  int rows = slot->bitmap.rows;
  int cols = slot->bitmap.width;

  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < cols; ++j) {
      int off = i * slot->bitmap.pitch + j / 8;
      if (slot->bitmap.buffer[off] & (0xC0 >> (j % 8))) {
        int r = pos.y - (rows - 1 - i);
        int c = pos.x + j;

        cv::Mat img = frame->ImageBGR();
        if (r >= 0 && r < static_cast<int>(frame->buf_surf->GetHeight()) &&
            c >= 0 && c < static_cast<int>(frame->buf_surf->GetWidth())) {
          cv::Vec3b pixel = img.at<cv::Vec3b>(cv::Point(c, r));
          cv::Scalar scalar = cv::Scalar(pixel.val[0], pixel.val[1], pixel.val[2]);

          // Color fusion
          float p = m_fontDiaphaneity;
          for (int k = 0; k < 4; ++k) {
            scalar.val[k] = scalar.val[k] * (1 - p) + color.val[k] * p;
          }
          img.at<cv::Vec3b>(cv::Point(c, r))[0] = (uint8_t)(scalar.val[0]);
          img.at<cv::Vec3b>(cv::Point(c, r))[1] = (uint8_t)(scalar.val[1]);
          img.at<cv::Vec3b>(cv::Point(c, r))[2] = (uint8_t)(scalar.val[2]);
        }
      }
    }
  }
  // Modify the output position of the next word
  double space = m_fontSize.val[0] * m_fontSize.val[1];
  double sep = m_fontSize.val[0] * m_fontSize.val[2];

  pos.x += static_cast<int>((cols ? cols : space) + sep);
}

int UniFont::putText(char* text, cv::Scalar color, cv::Scalar bg_color, void* bitmap, cv::Size size) {
  if (text == nullptr || bitmap == nullptr) {
    LOGE(OSD) << "[UniFont] [putText] text or bitmap is nullptr.";
  }

  std::unique_lock<std::mutex> guard(mutex_);
  if (!is_initialized_) {
    LOGE(OSD) << " [Osd] Please init UniFont first.";
    return -1;
  }

  wchar_t* w_str;
  if (ToWchar(text, w_str) == -1) {
    LOGE(OSD) << "[UniFont] [putText] [ToWchar] failed.";
    return -1;
  }

  cv::Point pos(0, size.height - 1);
  for (int i = 0; w_str[i] != '\0'; ++i) {
    putWChar(w_str[i], pos, color, bg_color, bitmap, size);
  }

  return 0;
}

// Output the current character and update the m pos position
void UniFont::putWChar(wchar_t wc, cv::Point& pos, cv::Scalar color, cv::Scalar bg_color, void* bitmap, cv::Size size) {
  // Generate a binary bitmap of a font based on unicode
  FT_UInt glyph_index = FT_Get_Char_Index(m_face, wc);
  FT_Load_Glyph(m_face, glyph_index, FT_LOAD_DEFAULT);
  FT_Render_Glyph(m_face->glyph, FT_RENDER_MODE_MONO);

  FT_GlyphSlot slot = m_face->glyph;

  // Cols and rows
  int rows = slot->bitmap.rows;
  int cols = slot->bitmap.width;

  uint8_t* base = static_cast<uint8_t*>(bitmap);
  uint32_t pitch = (size.width * 2 + 63) / 64 * 64;
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < cols; ++j) {
      int off = i * slot->bitmap.pitch + j / 8;
      if (slot->bitmap.buffer[off] & (0xC0 >> (j % 8))) {
        int r = pos.y - (rows - 1 - i);
        int c = pos.x + j;
        if (r >= 0 && r < size.height && c >= 0 && c < size.width) {
          uint8_t* pixel = base + r * pitch + c * 2;

#if 0
          cv::Scalar scalar = bg_color;
           // Color fusion
          float p = m_fontDiaphaneity;
          for (int k = 0; k < 4; ++k) {
            scalar.val[k] = scalar.val[k] * (1 - p) + color.val[k] * p;
          }
#else
          cv::Scalar scalar = color;
#endif
          uint8_t b = static_cast<uint8_t>(scalar.val[0]);
          uint8_t g = static_cast<uint8_t>(scalar.val[1]);
          uint8_t r = static_cast<uint8_t>(scalar.val[2]);
          uint16_t argb1555 =
              0x8000 + (b >> 3) + (static_cast<uint16_t>(g >> 3) << 5) + (static_cast<uint16_t>(r >> 3) << 10);
          *reinterpret_cast<uint16_t*>(pixel) = argb1555;
        }
      }
    }
  }
  // Modify the output position of the next word
  double space = m_fontSize.val[0] * m_fontSize.val[1];
  double sep = m_fontSize.val[0] * m_fontSize.val[2];

  pos.x += static_cast<int>((cols ? cols : space) + sep);
}

#endif

}  // namespace unistream
