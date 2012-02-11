#include "zone.h"

#include <sys/types.h> // size_t

namespace dotlang {

Zone* Zone::current_ = NULL;

void* Zone::Allocate(size_t size) {
  if (blocks_.Head()->value()->has(size)) {
    return blocks_.Head()->value()->allocate(size);
  } else {
    size_t block_size = size;

    if (block_size % page_size_ != 0) {
      block_size += page_size_ - block_size % page_size_;
    }

    ZoneBlock* block = new ZoneBlock(page_size_);
    blocks_.Unshift(block);
    return block->allocate(size);
  }
}

} // namespace dotlang
