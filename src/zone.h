/**
 * Copyright (c) 2012, Fedor Indutny.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _SRC_ZONE_H_
#define _SRC_ZONE_H_

#include <stdlib.h>  // malloc, free, abort
#include <sys/types.h>  // size_t
#include <assert.h>  // assert

#include "utils.h"  // List

namespace candor {
namespace internal {

// Chunk of memory that will hold some of allocated data in Zone
class ZoneBlock {
 public:
  explicit ZoneBlock(size_t size) : data_(NULL), offset_(0), size_(size) {
    data_ = malloc(size);
    if (data_ == NULL) abort();
  }

  ~ZoneBlock() {
    free(data_);
  }

  inline bool has(size_t bytes) {
    return offset_ + bytes <= size_;
  }

  inline void* allocate(size_t bytes) {
    void* result = reinterpret_cast<char*>(data_) + offset_;
    offset_ += bytes;
    assert(has(0));
    return result;
  }

  void* data_;
  size_t offset_;
  size_t size_;
};

// Zones are used for AST and other data that should be allocated fast
// and removed at once. Each allocation goes into zone's block or new block
// is created (if current one was exhausted).
class Zone {
 public:
  class ZoneItem {
   public:
    // Just a stub
  };

  Zone() {
    // TODO(indutny): this should use thread id
    parent_ = current_;
    current_ = this;

    page_size_ = GetPageSize();

    blocks_.Push(new ZoneBlock(10 * page_size_));
  }

  ~Zone() {
    current_ = parent_;
  }

  void* Allocate(size_t size);

  static Zone* current_;
  static inline Zone* current() { return current_; }

  Zone* parent_;

  List<ZoneBlock*, ZoneItem> blocks_;

  size_t page_size_;
};

class ZonePolicy {
 public:
  static void* Allocate(size_t size);
};

// Base class for objects that will be bound to some zone
class ZoneObject {
 public:
  inline void* operator new(size_t size) {
    return Zone::current()->Allocate(size);
  }

  inline void operator delete(void*, size_t size) {
    // This may be called in list, just ignore it
  }
};

template <class T>
class ZoneList : public GenericList<T, ZoneObject, NopPolicy> {
 public:
};

}  // namespace internal
}  // namespace candor

#endif  // _SRC_ZONE_H_
