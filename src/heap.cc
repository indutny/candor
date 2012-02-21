#include "heap.h"
#include <stdint.h> // uint32_t
#include <assert.h> // assert

namespace dotlang {

Space::Space(uint32_t page_size) : page_size_(page_size) {
  // Create the first page
  pages_.Push(new Page(page_size));
  pages_.allocated = true;

  select(pages_.head()->value());
}


void Space::select(Page* page) {
  top_ = &page->top_;
  limit_ = &page->limit_;
}


char* Space::Allocate(uint32_t bytes) {
  // This function should be called only if current page
  // was exhausted
  assert(*top_ + bytes > *limit_);

  // Go through all pages to find gap
  List<Page*, EmptyClass>::Item* item = pages_.head();
  while (*top_ + bytes > *limit_ && item->next() != NULL) {
    item = item->next();
    select(item->value());
  }

  // No gap was found - allocate new page
  if (*top_ + bytes > *limit_) {
    Page* next = new Page(RoundUp(bytes, page_size_));
    pages_.Push(next);
    select(next);
  }

  char* result = *top_;

  *top_ += bytes;

  return result;
}


HNumber* HNumber::Cast(void* value) {
  assert(*reinterpret_cast<uint64_t*>(value) == Heap::kNumber);
  return new HNumber(*(reinterpret_cast<int64_t*>(value) + 1));
}

} // namespace dotlang
