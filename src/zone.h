#ifndef _SRC_ZONE_H_
#define _SRC_ZONE_H_

#include "utils.h" // List
#include <stdlib.h> // malloc, free, abort
#include <sys/types.h> // size_t
#include <unistd.h> // sysconf or getpagesize
#include <assert.h> // assert

namespace dotlang {

class ZoneBlock {
 public:
  ZoneBlock(size_t size) : data_(NULL), offset_(0), size_(size) {
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

class Zone {
 public:
  class ZoneItem {
   public:
    // Just a stub
  };

  Zone() {
    // TODO: this should use thread id
    current_ = this;
    blocks_.allocated = true;

#ifdef __DARWIN
    page_size_ = getpagesize();
#else
    page_size_ = sysconf(_SC_PAGE_SIZE);
#endif

    blocks_.Push(new ZoneBlock(page_size_));
  }

  void* Allocate(size_t size);

  static Zone* current_;

  List<ZoneBlock*, ZoneItem> blocks_;

  size_t page_size_;
};

class ZoneObject {
 public:
  inline void* operator new(size_t size) {
    return Zone::current_->Allocate(size);
  }

  inline void operator delete(void*, size_t size) {
    // This may be called in list, just ignore it
  }
};

} // namespace dotlang

#endif _SRC_ZONE_H_
