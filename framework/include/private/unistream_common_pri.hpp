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

#ifndef UNISTREAM_COMMON_PRI_HPP_
#define UNISTREAM_COMMON_PRI_HPP_

#include <string.h>
#include <unistd.h>

#include <string>
#include <vector>

#if defined(__GNUC__) || defined(__clang__)
#define UNIS_DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER)
#define UNIS_DEPRECATED __declspec(deprecated)
#else
#error You need to implement UNIS_DEPRECATED for this compiler
#define UNIS_DEPRECATED
#endif

#if defined(__GNUC__)
#define UNIS_IGNORE_DEPRECATED_PUSH \
  _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#define UNIS_IGNORE_DEPRECATED_POP _Pragma("GCC diagnostic pop")
#elif defined(__clang__)
#define UNIS_IGNORE_DEPRECATED_PUSH \
  _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"")
#define UNIS_IGNORE_DEPRECATED_POP _Pragma("clang diagnostic pop")
#elif defined(_MSC_VER) && _MSC_VER >= 1400
#define UNIS_IGNORE_DEPRECATED_PUSH \
  __pragma(warning(push)) __pragma(warning(disable : 4996)) #define UNIS_IGNORE_DEPRECATED_POP __pragma(warning(pop))
#else
#error You need to implement UNIS_IGNORE_DEPRECATED_PUSH and  \
    UNIS_IGNORE_DEPRECATED_POP for this compiler
#define UNIS_IGNORE_DEPRECATED_PUSH
#define UNIS_IGNORE_DEPRECATED_POP
#endif

namespace unistream {
/*!
 * @enum UNIPixelFormat
 *
 * @brief Enumeration variables describing the picture formats
 */
enum class UNIPixelFormat {
  YUV420P = 0,  /*!< The format with planar Y4-U1-V1, I420 */
  RGB24,        /*!< The format with packed R8G8B8 */
  BGR24,        /*!< The format with packed B8G8R8 */
  NV21,         /*!< The format with semi-Planar Y4-V1U1 */
  NV12,         /*!< The format with semi-Planar Y4-U1V1 */
  I422,         /*!< The format with semi-Planar I422 */
  I444,         /*!< The format with semi-Planar I444 */
};

/*!
 * @enum UNICodecType
 *
 * @brief Enumeration variables describing the codec types
 */
enum class UNICodecType {
  H264 = 0,  /*!< The H264 codec type */
  HEVC,      /*!< The HEVC codec type */
  MPEG4,     /*!< The MPEG4 codec type */
  JPEG       /*!< The JPEG codec type */
};

/*!
 * @class NonCopyable
 *
 * @brief NonCopyable is the abstraction of the class which has no ability to do copy and assign. It is always be used
 *        as the base class to disable copy and assignment.
 */
class NonCopyable {
 protected:
  /*!
   * @brief Constructs an instance with empty value.
   *
   * @param None.
   *
   * @return  None.
   */
  NonCopyable() = default;
  /*!
   * @brief Destructs an instance.
   *
   * @param None.
   *
   * @return  None.
   */
  ~NonCopyable() = default;

 private:
  NonCopyable(const NonCopyable &) = delete;
  NonCopyable(NonCopyable &&) = delete;
  NonCopyable &operator=(const NonCopyable &) = delete;
  NonCopyable &operator=(NonCopyable &&) = delete;
};

constexpr size_t kInvalidModuleId = (size_t)(-1);
constexpr uint32_t kInvalidStreamIdx = (uint32_t)(-1);
static constexpr uint32_t kMaxStreamNum = 128; /*!< The streams at most allowed. */

#define UNIS_JSON_DIR_PARAM_NAME "json_file_dir"

/**
 * @brief Profiler configuration title in JSON configuration file.
 **/
static constexpr char kProfilerConfigName[] = "profiler_config";
/**
 * @brief Subgraph node item prefix.
 **/
static constexpr char kSubgraphConfigPrefix[] = "subgraph:";

/**
 *
 * @brief Judges if the configuration item name represents a subgraph.
 *
 * @param[in] item_name The item name.
 *
 * @return Returns true if the ``item_name`` represents a subgraph. Otherwise, returns false.
 **/
inline bool IsSubgraphItem(const std::string &item_name) {
  return item_name.size() > strlen(kSubgraphConfigPrefix) &&
         kSubgraphConfigPrefix == item_name.substr(0, strlen(kSubgraphConfigPrefix));
}

/**
 * @brief Checks one stream whether reaches EOS.
 *
 * @param[in] stream_id The identifier of a stream.
 * @param[in] sync The mode of checking the status. True means checking in synchronized mode while False represents
 *                 for asynchronous.
 *
 * @return Returns true if the EOS reached, otherwise returns false.
 *
 * @note It's used for removing sources forcedly.
 */
bool CheckStreamEosReached(const std::string &stream_id, bool sync = true);
/**
 * @brief Checks one stream whether reaches EOS.
 *
 * @param[in] stream_id The identifier of a stream.
 * @param[in] value The status of a stream.
 *
 * @return No return value.
 *
 * @note It's used for removing sources forcedly.
 */
void SetStreamRemoved(const std::string &stream_id, bool value = true);
/**
 * @brief Checks whether a stream is removed.
 *
 * @param[in] stream_id The identifier of a stream.
 *
 * @return Returns true if the stream is removed, otherwise returns false.
 *
 * @note It's used for removing sources forcedly.
 */
bool IsStreamRemoved(const std::string &stream_id);

}  // namespace unistream

#endif  // UNISTREAM_COMMON_PRI_HPP_
