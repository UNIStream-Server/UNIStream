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
#include "private/unistream_allocator.hpp"

#include <exception>
#include <memory>

namespace unistream {

MluDeviceGuard::MluDeviceGuard(int device_id) : device_id_(device_id) {
  if (device_id < 0) {
    // means use cpu, do nothing.
  } else {
    unsigned int dev_num = 0;
    if (dev_num < 1) {
      LOGE(CORE) << "There is no valid device";
    } else if (device_id > static_cast<int>(dev_num - 1)) {
      LOGE(CORE) << "The device ID: " << device_id << "must be less than " << dev_num;
    } else {
    }
  }
}

MluDeviceGuard::~MluDeviceGuard() {}

class MemoryAllocator : public NonCopyable {
 public:
  explicit MemoryAllocator(int device_id) : device_id_(device_id) {}
  virtual ~MemoryAllocator() = default;
  virtual void *alloc(size_t size, int timeout_ms = 0) = 0;
  virtual void free(void *p) = 0;
  int device_id() const { return device_id_; }
  void set_device_id(int device_id) { device_id_ = device_id; }

 protected:
  int device_id_ = -1;
  std::mutex mutex_;
};

class CpuAllocator : public MemoryAllocator {
 public:
  CpuAllocator() : MemoryAllocator(-1) {}
  ~CpuAllocator() = default;

  void *alloc(size_t size, int timeout_ms = 0) override;
  void free(void *p) override;
};

class MluAllocator : public MemoryAllocator {
 public:
  explicit MluAllocator(int device_id = 0) : MemoryAllocator(device_id) {}
  ~MluAllocator() = default;

  void *alloc(size_t size, int timeout_ms = 0) override;
  void free(void *p) override;
};

// helper funcs
class UniAllocDeleter final {
 public:
  explicit UniAllocDeleter(std::shared_ptr<MemoryAllocator> allocator) : allocator_(allocator) {}

  void operator()(void *ptr) { allocator_->free(ptr); }

 private:
  std::shared_ptr<MemoryAllocator> allocator_;
};

std::shared_ptr<void> uniMemAlloc(size_t size, std::shared_ptr<MemoryAllocator> allocator) {
  if (allocator) {
    std::shared_ptr<void> ds(allocator->alloc(size), UniAllocDeleter(allocator));
    return ds;
  }
  return nullptr;
}

std::shared_ptr<void> uniCpuMemAlloc(size_t size) {
  std::shared_ptr<MemoryAllocator> allocator = std::make_shared<CpuAllocator>();
  return uniMemAlloc(size, allocator);
}

std::shared_ptr<void> uniMluMemAlloc(size_t size, int device_id) {
  std::shared_ptr<MemoryAllocator> allocator = std::make_shared<MluAllocator>(device_id);
  return uniMemAlloc(size, allocator);
}

// cpu Var-size allocator
void *CpuAllocator::alloc(size_t size, int timeout_ms) {
  size_t alloc_size = (size + 4095) & (~0xFFF);  // Align 4096
  return static_cast<void *>(new (std::nothrow) unsigned char[alloc_size]);
}

void CpuAllocator::free(void *p) {
  unsigned char *ptr = static_cast<unsigned char *>(p);
  delete[] ptr;
}

// mlu var-size allocator
void *MluAllocator::alloc(size_t size, int timeout_ms) {
  //size_t alloc_size = (size + 4095) & (~0xFFF);  // Align 4096

  std::lock_guard<std::mutex> lk(mutex_);
  MluDeviceGuard guard(device_id_);
  void *mlu_ptr = nullptr;
  return mlu_ptr;
}

void MluAllocator::free(void *p) {
  std::lock_guard<std::mutex> lk(mutex_);
  MluDeviceGuard guard(device_id_);
}

}  // namespace unistream
