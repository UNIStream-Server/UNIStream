/*************************************************************************
* Copyright (C) [2019-2022] by Cambricon, Inc. All rights reserved
*
* This source code is licensed under the Apache-2.0 license found in the
* LICENSE file in the root directory of this source tree.
*
* A part of this source code is referenced from glog project.
* https://github.com/google/glog/blob/master/src/logging.cc
*
* Copyright (c) 1999, Google Inc.
*
* This source code is licensed under the BSD 3-Clause license found in the
* LICENSE file in the root directory of this source tree.
*
*************************************************************************/

#ifndef UNISTREAM_CORE_LOGGING_HPP_
#define UNISTREAM_CORE_LOGGING_HPP_
#include <glog/logging.h>

#define LOGF(tag) LOG(FATAL) << "[UNIStream " << (#tag) << " FATAL] "
#define LOGE(tag) LOG(ERROR) << "[UNIStream " << (#tag) << " ERROR] "
#define LOGW(tag) LOG(WARNING) << "[UNIStream " << (#tag) << " WARN] "
#define LOGI(tag) LOG(INFO) << "[UNIStream " << (#tag) << " INFO] "
#define VLOG1(tag) VLOG(1) << "[UNIStream " << (#tag) << " V1] "
#define VLOG2(tag) VLOG(2) << "[UNIStream " << (#tag) << " V2] "
#define VLOG3(tag) VLOG(3) << "[UNIStream " << (#tag) << " V3] "
#define VLOG4(tag) VLOG(4) << "[UNIStream " << (#tag) << " V4] "
#define VLOG5(tag) VLOG(5) << "[UNIStream " << (#tag) << " V5] "

#define LOGF_IF(tag, condition) LOG_IF(FATAL, condition) << "[UNIStream " << (#tag) << " FATAL] "
#define LOGE_IF(tag, condition) LOG_IF(ERROR, condition) << "[UNIStream " << (#tag) << " ERROR] "
#define LOGW_IF(tag, condition) LOG_IF(WARNING, condition) << << "[UNIStream " << (#tag) << " WARN] "
#define LOGI_IF(tag, condition) LOG_IF(INFO, condition) << "[UNIStream " << (#tag) << " INFO] "
#define VLOG1_IF(tag, condition) VLOG_IF(1, condition) << "[UNIStream " << (#tag) << " V1] "
#define VLOG2_IF(tag, condition) VLOG_IF(2, condition) << "[UNIStream " << (#tag) << " V2] "
#define VLOG3_IF(tag, condition) VLOG_IF(3, condition) << "[UNIStream " << (#tag) << " V3] "
#define VLOG4_IF(tag, condition) VLOG_IF(4, condition) << "[UNIStream " << (#tag) << " V4] "
#define VLOG5_IF(tag, condition) VLOG_IF(5, condition) << "[UNIStream " << (#tag) << " V5] "

#endif  // UNISTREAM_CORE_LOGGING_HPP_