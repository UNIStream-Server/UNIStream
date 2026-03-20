/*************************************************************************
 * Copyright (C) [2020] by Cambricon, Inc. All rights reserved
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

#ifndef MODULES_KAFKA_INCLUDE_HANDLER_HPP_
#define MODULES_KAFKA_INCLUDE_HANDLER_HPP_

#include <memory>
#include <string>

#include "unistream_frame.hpp"
#include "unistream_frame_va.hpp"

#include "reflex_object.h"

namespace unistream {

class KafkaClient;

class KafkaHandler : virtual public ReflexObjectEx<KafkaHandler> {
 public:
  static KafkaHandler *Create(const std::string &name);
  virtual ~KafkaHandler() {}

  virtual int UpdateFrame(const UNIFrameInfoPtr &data) { return 0; }

  friend class Kafka;

 protected:
  bool Produce(const std::string &content);
  bool Consume(std::string *content, int timeout_ms);

 private:
  std::string brokers_;
  std::string topic_;
  std::unique_ptr<KafkaClient> producer_ = nullptr;
  std::unique_ptr<KafkaClient> consumer_ = nullptr;
};  // class KafkaHandler

}  // namespace unistream

#endif  // ifndef MODULES_KAFKA_INCLUDE_HANDLER_HPP_
