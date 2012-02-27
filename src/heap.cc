#include "heap.h"
#include "runtime.h" // RuntimeCompare

#include <stdint.h> // uint32_t
#include <stdlib.h> // NULL
#include <string.h> // memcpy
#include <zone.h> // Zone::Allocate
#include <assert.h> // assert

namespace dotlang {

Heap* Heap::current_ = NULL;

Space::Space(Heap* heap, uint32_t page_size) : heap_(heap),
                                               page_size_(page_size) {
  // Create the first page
  pages_.Push(new Page(page_size));
  pages_.allocated = true;

  select(pages_.head()->value());
}


void Space::select(Page* page) {
  top_ = &page->top_;
  limit_ = &page->limit_;
}


char* Space::Allocate(uint32_t bytes, char* context) {
  // If current page was exhausted - run GC
  bool need_gc = *top_ + bytes > *limit_;

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

  if (need_gc) {
    Zone gc_zone;
    heap()->gc()->CollectGarbage(HValue::As<HContext>(context));
  }

  return result;
}


char* Heap::AllocateTagged(HeapTag tag, uint32_t bytes, char* context) {
  char* result = new_space()->Allocate(bytes + 8, context);
  *reinterpret_cast<uint64_t*>(result) = tag;

  return result;
}


HValue::HValue(Heap* heap, char* addr) : addr_(addr), heap_(heap) {
  tag_ = static_cast<Heap::HeapTag>(*reinterpret_cast<uint64_t*>(addr));
}


HValue::HValue(char* addr) : addr_(addr), heap_(Heap::Current()) {
  // XXX: A little bit of copy-paste here
  tag_ = static_cast<Heap::HeapTag>(*reinterpret_cast<uint64_t*>(addr));
}


HContext::HContext(char* addr) : HValue(addr) {
  slots_ = *reinterpret_cast<uint64_t*>(addr + 8);
}


HNumber::HNumber(char* addr) : HValue(addr) {
  value_ = *reinterpret_cast<int64_t*>(addr + 8);
}


HString::HString(char* addr) : HValue(addr) {
  length_ = *reinterpret_cast<uint32_t*>(addr + 16);
  value_ = addr + 24;

  // Compute hash lazily
  uint32_t* hash_addr = reinterpret_cast<uint32_t*>(addr + 8);
  hash_ = *hash_addr;
  if (hash_ == 0) {
    hash_ = ComputeHash(value_, length_);
    *hash_addr = hash_;
  }
}


HObject::HObject(Heap* heap, char* addr) : HValue(heap, addr) {
}


HFunction::HFunction(char* addr) : HValue(addr) {
}


} // namespace dotlang
