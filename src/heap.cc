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


char* Space::Allocate(uint32_t bytes, char* stack_top) {
  // If current page was exhausted - run GC
  bool need_gc = stack_top != NULL && *top_ + bytes > *limit_;

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
    heap()->gc()->CollectGarbage(stack_top);
  }

  return result;
}


void Space::Swap(Space* space) {
  // Remove self pages
  Clear();

  while (space->pages_.length() != 0) {
    pages_.Push(space->pages_.Shift());
  }
}


void Space::Clear() {
  while (pages_.length() != 0) {
    delete pages_.Shift();
  }
}


char* Heap::AllocateTagged(HeapTag tag, uint32_t bytes, char* stack_top) {
  char* result = new_space()->Allocate(bytes + 8, stack_top);
  *reinterpret_cast<uint64_t*>(result) = tag;

  return result;
}


Heap::HeapTag HValue::GetTag(char* addr) {
  return static_cast<Heap::HeapTag>(*reinterpret_cast<uint8_t*>(addr));
}


HValue::HValue(char* addr) : addr_(addr) {
  tag_ = HValue::GetTag(addr);
}


HValue* HValue::New(char* addr) {
  switch (HValue::GetTag(addr)) {
   case Heap::kTagContext:
    return HValue::As<HContext>(addr);
   case Heap::kTagFunction:
    return HValue::As<HFunction>(addr);
   case Heap::kTagNumber:
    return HValue::As<HNumber>(addr);
   case Heap::kTagString:
    return HValue::As<HString>(addr);
   case Heap::kTagObject:
    return HValue::As<HObject>(addr);
   case Heap::kTagMap:
    return HValue::As<HMap>(addr);
   default:
    assert(0 && "Not implemented");
  }
  return NULL;
}


HValue* HValue::CopyTo(Space* space) {
  uint32_t size = 8;
  switch (tag()) {
   case Heap::kTagContext:
    // parent + slots
    size += 16 + As<HContext>()->slots() * 8;
    break;
   case Heap::kTagFunction:
    // parent + body
    size += 16;
    break;
   case Heap::kTagNumber:
    // value
    size += 8;
    break;
   case Heap::kTagString:
    // hash + length + bytes
    size += 16 + As<HString>()->length();
    break;
   case Heap::kTagObject:
    // mask + map
    size += 16;
    break;
   case Heap::kTagMap:
    // size + space ( keys + values )
    size += 8 + As<HMap>()->size() << 4;
    break;
   default:
    assert(0 && "Unexpected");
  }

  char* result = space->Allocate(size, NULL);
  memcpy(result, addr(), size);

  return HValue::New(result);
}


bool HValue::IsGCMarked() {
  return (*reinterpret_cast<uint64_t*>(addr()) & 0x100) == 0x100;
}


char* HValue::GetGCMark() {
  assert(IsGCMarked());
  return *reinterpret_cast<char**>(addr() + 8);
}


void HValue::SetGCMark(char* new_addr) {
  *reinterpret_cast<uint64_t*>(addr()) |= 0x100;
  *reinterpret_cast<char**>(addr() + 8) = new_addr;
}


void HValue::ResetGCMark() {
  if (IsGCMarked()) {
    *reinterpret_cast<uint64_t*>(addr()) ^= 0x100;
  }
}


HContext::HContext(char* addr) : HValue(addr) {
  parent_slot_ = reinterpret_cast<char**>(addr + 8);
  slots_ = *reinterpret_cast<uint64_t*>(addr + 16);
}


bool HContext::HasParent() {
  return *parent_slot_ != NULL;
}


bool HContext::HasSlot(uint32_t index) {
  return *GetSlotAddress(index) != NULL;
}


HValue* HContext::GetSlot(uint32_t index) {
  return HValue::New(*GetSlotAddress(index));
}


char** HContext::GetSlotAddress(uint32_t index) {
  return reinterpret_cast<char**>(addr() + 24 + index * 8);
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


HObject::HObject(char* addr) : HValue(addr) {
  map_slot_ = reinterpret_cast<char**>(addr + 16);
}


HMap::HMap(char* addr) : HValue(addr) {
  size_ = *reinterpret_cast<uint64_t*>(addr + 8);
  space_ = addr + 16;
}


bool HMap::IsEmptySlot(uint32_t index) {
  return *GetSlotAddress(index) == NULL;
}


HValue* HMap::GetSlot(uint32_t index) {
  return HValue::New(*GetSlotAddress(index));
}


char** HMap::GetSlotAddress(uint32_t index) {
  return reinterpret_cast<char**>(space_ + index * 8);
}


HFunction::HFunction(char* addr) : HValue(addr) {
  parent_slot_ = reinterpret_cast<char**>(addr + 8);
}


} // namespace dotlang
