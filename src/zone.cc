#include "zone.h"
#include "utils.h" // RoundUp

#include <sys/types.h> // size_t

namespace dotlang {

Zone* Zone::current_ = NULL;

void* Zone::Allocate(size_t size) {
  // If current block has enough size - allocate chunk in it
  if (blocks_.head()->value()->has(size)) {
    return blocks_.head()->value()->allocate(size);
  } else {
    // Otherwise create new block that will definitely fit that chunk
    ZoneBlock* block = new ZoneBlock(RoundUp(size, page_size_));
    blocks_.Unshift(block);
    return block->allocate(size);
  }
}

} // namespace dotlang
