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
  uint32_t even_bytes = bytes + (bytes & 0x01);
  bool place_in_current = *top_ + even_bytes <= *limit_;
  bool need_gc = stack_top != NULL && !place_in_current;

  if (need_gc) {
    Zone gc_zone;
    heap()->gc()->CollectGarbage(stack_top);
  }

  if (!place_in_current) {
    // Go through all pages to find gap
    List<Page*, EmptyClass>::Item* item = pages_.head();
    while (*top_ + even_bytes > *limit_ && item->next() != NULL) {
      item = item->next();
      select(item->value());
    }

    // No gap was found - allocate new page
    if (item->next() == NULL) {
      Page* next = new Page(RoundUp(even_bytes, page_size_));
      pages_.Push(next);
      select(next);
    }
  }

  char* result = *top_;
  *top_ += even_bytes;

  return result;
}


void Space::Swap(Space* space) {
  // Remove self pages
  Clear();

  while (space->pages_.length() != 0) {
    pages_.Push(space->pages_.Shift());
  }

  select(pages_.head()->value());
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
  if (addr == NULL) return Heap::kTagNil;

  if (IsUnboxed(addr)) {
    return Heap::kTagNumber;
  }
  return static_cast<Heap::HeapTag>(*reinterpret_cast<uint8_t*>(addr));
}


bool HValue::IsUnboxed(char* addr) {
  return (reinterpret_cast<uint64_t>(addr) & 0x01) == 0x01;
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
   case Heap::kTagBoolean:
    return HValue::As<HBoolean>(addr);
   case Heap::kTagString:
    return HValue::As<HString>(addr);
   case Heap::kTagObject:
    return HValue::As<HObject>(addr);
   case Heap::kTagMap:
    return HValue::As<HMap>(addr);
   case Heap::kTagNil:
    // Nil has a NULL address
    assert(0 && "Unexpected");
   case Heap::kTagCode:
    return NULL;
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
    // TODO: Implement real
    // value
    assert(0 && "Not implemented");
    break;
   case Heap::kTagBoolean:
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
    size += 8 + (As<HMap>()->size() << 4);
    break;
   default:
    assert(0 && "Unexpected");
  }

  char* result = space->Allocate(size, NULL);
  memcpy(result, addr(), size);

  return HValue::New(result);
}


bool HValue::IsGCMarked() {
  if (IsUnboxed(addr())) return false;
  return (*reinterpret_cast<uint64_t*>(addr()) & 0x80000000) == 0x80000000;
}


char* HValue::GetGCMark() {
  assert(IsGCMarked());
  return *reinterpret_cast<char**>(addr() + 8);
}


void HValue::SetGCMark(char* new_addr) {
  *reinterpret_cast<uint64_t*>(addr()) |= 0x80000000;
  *reinterpret_cast<char**>(addr() + 8) = new_addr;
}


void HValue::ResetGCMark() {
  if (IsGCMarked()) {
    *reinterpret_cast<uint64_t*>(addr()) ^= 0x80000000;
  }
}


HContext::HContext(char* addr) : HValue(addr) {
  parent_slot_ = reinterpret_cast<char**>(addr + 8);
  slots_ = *reinterpret_cast<uint64_t*>(addr + 16);
}


char* HContext::New(Heap* heap,
                    char* stack_top,
                    List<char*, ZoneObject>* values) {
  char* result = heap->AllocateTagged(Heap::kTagContext,
                                      16 + values->length() * 8,
                                      stack_top);

  // Zero parent
  *reinterpret_cast<char**>(result + 8) = NULL;

  // Put size
  *reinterpret_cast<uint64_t*>(result + 16) = values->length();

  // Put all values
  char* slot = result + 24;
  while (values->length() != 0) {
    *reinterpret_cast<char**>(slot) = values->Shift();
    slot += 8;
  }

  return result;
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
  if ((reinterpret_cast<uint64_t>(addr) & 0x01) == 0x01) {
    // Unboxed value
    value_ = reinterpret_cast<int64_t>(addr) >> 1;
  } else {
    assert(0 && "Not implemented yet");
  }
}


char* HNumber::New(Heap* heap, char* stack_top, int64_t value) {
  return reinterpret_cast<char*>(Tag(value));
}


int64_t HNumber::Untag(int64_t value) {
  return value >> 1;
}


int64_t HNumber::Tag(int64_t value) {
  return (value << 1) | 1;
}


HBoolean::HBoolean(char* addr) : HValue(addr) {
  value_ = *reinterpret_cast<int8_t*>(addr + 8) == 1;
}


char* HBoolean::New(Heap* heap, char* stack_top, bool value) {
  char* result = heap->AllocateTagged(Heap::kTagBoolean, 8, stack_top);
  *reinterpret_cast<int8_t*>(result + 8) = value ? 1 : 0;

  return result;
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


char* HString::New(Heap* heap,
                   char* stack_top,
                   const char* value,
                   uint32_t length) {
  char* result = heap->AllocateTagged(Heap::kTagString, length + 24, stack_top);

  // Zero hash
  *reinterpret_cast<uint64_t*>(result + 8) = 0;
  // Set length
  *reinterpret_cast<uint64_t*>(result + 16) = length;
  // Copy value
  memcpy(result + 24, value, length);

  return result;
}


HObject::HObject(char* addr) : HValue(addr) {
  map_slot_ = reinterpret_cast<char**>(addr + 16);
}


char* HObject::NewEmpty(Heap* heap, char* stack_top) {
  uint32_t size = 128;

  char* obj = heap->AllocateTagged(Heap::kTagObject, 16, stack_top);
  // NOTE: We're not passing stack_top here, because we don't want
  // GC to run until all object's fields will be filled
  char* map = heap->AllocateTagged(Heap::kTagMap, (size << 4) + 8, NULL);

  // Set mask
  *reinterpret_cast<uint64_t*>(obj + 8) = (size - 1) << 3;
  // Set map
  *reinterpret_cast<char**>(obj + 16) = map;

  // Set map's size
  *reinterpret_cast<uint64_t*>(map + 8) = size;

  // Nullify all map's slots (both keys and values)
  memset(map + 16, 0, size << 4);

  return obj;
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
