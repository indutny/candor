#include "heap.h"
#include <stdint.h> // uint32_t
#include <string.h> // memcpy
#include <zone.h> // Zone::Allocate
#include <assert.h> // assert

namespace dotlang {

Heap* Heap::current_ = NULL;

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


HValue::HValue(char* addr) : addr_(addr) {
  tag_ = static_cast<Heap::HeapTag>(*reinterpret_cast<uint64_t*>(addr));
  heap_ = Heap::Current();
}


HNumber::HNumber(char* addr) : HValue(addr) {
  value_ = *reinterpret_cast<int64_t*>(addr + 8);
}


HString::HString(char* addr) : HValue(addr) {
  hash_ = *reinterpret_cast<uint32_t*>(addr + 8);
  length_ = *reinterpret_cast<uint32_t*>(addr + 16);
  value_ = *reinterpret_cast<char**>(addr + 24);
}

HFunction::HFunction(char* addr) : HValue(addr) {
}

} // namespace dotlang
