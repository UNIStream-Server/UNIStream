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

#ifndef UNISTREAM_FRAME_HPP_
#define UNISTREAM_FRAME_HPP_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "unistream_collection.hpp"
#include "unistream_common.hpp"
#include "util/unistream_any.hpp"
#include "util/unistream_rwlock.hpp"

/**
 *  @file unistream_frame.hpp
 *
 *  This file contains a declaration of the UNIFrameInfo class.
 */
namespace unistream {

class Module;
class Pipeline;

/**
 * @enum UNIFrameFlag
 *
 * @brief Enumeration variables describing the mask of UNIDataFrame.
 */
enum class UNIFrameFlag {
  UNI_FRAME_FLAG_EOS = 1 << 0,     /*!< This enumeration indicates the end of data stream. */
  UNI_FRAME_FLAG_INVALID = 1 << 1, /*!< This enumeration indicates an invalid frame. */
  UNI_FRAME_FLAG_REMOVED = 1 << 2  /*!< This enumeration indicates that the stream has been removed. */
};

/**
 * @class UNIFrameInfo
 *
 * @brief UNIFrameInfo is a class holding the information of a frame.
 *
 */
class UNIFrameInfo : private NonCopyable {
 public:
  /**
   * @brief Creates a UNIFrameInfo instance.
   *
   * @param[in] stream_id The data stream alias. Identifies which data stream the frame data comes from.
   * @param[in] eos  Whether this is the end of the stream. This parameter is set to false by default to
   *                 create a UNIFrameInfo instance. If you set this parameter to true,
   *                 UNIDataFrame::flags will be set to ``UNI_FRAME_FLAG_EOS``. Then, the modules
   *                 do not have permission to process this frame. This frame should be handed over to
   *                 the pipeline for processing.
   *
   * @return Returns ``shared_ptr`` of ``UNIFrameInfo`` if this function has run successfully. Otherwise, returns NULL.
   */
  static std::shared_ptr<UNIFrameInfo> Create(const std::string& stream_id, bool eos = false,
                                             std::shared_ptr<UNIFrameInfo> payload = nullptr);

  UNIS_IGNORE_DEPRECATED_PUSH

 private:
  UNIFrameInfo() = default;

 public:
  /**
   * @brief Destructs UNIFrameInfo object.
   *
   * @return No return value.
   */
  ~UNIFrameInfo();
  UNIS_IGNORE_DEPRECATED_POP

  /**
   * @brief Checks whether DataFrame is end of stream (EOS) or not.
   *
   * @return Returns true if the frame is EOS. Returns false if the frame is not EOS.
   */
  bool IsEos() { return (flags & static_cast<size_t>(unistream::UNIFrameFlag::UNI_FRAME_FLAG_EOS)) ? true : false; }

  /**
   * @brief Checks whether DataFrame is removed or not.
   *
   * @return Returns true if the frame is EOS. Returns false if the frame is not EOS.
   */
  bool IsRemoved() {
    return (flags & static_cast<size_t>(unistream::UNIFrameFlag::UNI_FRAME_FLAG_REMOVED)) ? true : false;
  }

  /**
   * @brief Checks if DataFrame is valid or not.
   *
   *
   *
   * @return Returns true if frame is invalid, otherwise returns false.
   */
  bool IsInvalid() {
    return (flags & static_cast<size_t>(unistream::UNIFrameFlag::UNI_FRAME_FLAG_INVALID)) ? true : false;
  }

  /**
   * @brief Sets index (usually the index is a number) to identify stream.
   *
   * @param[in] index Number to identify stream.
   *
   * @return No return value.
   *
   * @note This is only used for distributing each stream data to the appropriate thread.
   * We do not recommend SDK users to use this API because it will be removed later.
   */
  void SetStreamIndex(uint32_t index) { channel_idx = index; }

  /**
   * @brief Gets index number which identifies stream.
   *
   *
   *
   * @return Index number.
   *
   * @note This is only used for distributing each stream data to the appropriate thread.
   * We do not recommend SDK users to use this API because it will be removed later.
   */
  uint32_t GetStreamIndex() const { return channel_idx; }

  std::string stream_id;        /*!< The data stream aliases where this frame is located to. */
  int64_t timestamp = -1;       /*!< The time stamp of this frame. */
  std::atomic<size_t> flags{0}; /*!< The mask for this frame, ``UNIFrameFlag``. */

  Collection collection;                                    /*!< Stored structured data.  */
  std::shared_ptr<unistream::UNIFrameInfo> payload = nullptr; /*!< UNIFrameInfo instance of parent pipeline. */

 private:
  /**
   * The below methods and members are used by the framework.
   */
  friend class Pipeline;
  mutable uint32_t channel_idx = kInvalidStreamIdx;  ///< The index of the channel, stream_index
  void SetModulesMask(uint64_t mask);
  uint64_t GetModulesMask();
  uint64_t MarkPassed(Module* current);  // return changed mask

  RwLock mask_lock_;
  /* Identifies which modules have processed this data */
  uint64_t modules_mask_ = 0;
};

/*!
 * Defines an alias for the std::shared_ptr<UNIFrameInfo>. UNIFrameInfoPtr now denotes a shared pointer of frame
 * information.
 */
using UNIFrameInfoPtr = std::shared_ptr<UNIFrameInfo>;

}  // namespace unistream

#endif  // UNISTREAM_FRAME_HPP_
