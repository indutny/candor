#include "heap.h"
#include "heap-inl.h"
#include "runtime.h" // RuntimeLookupProperty

#include <stdint.h> // uint32_t
#include <sys/types.h> // off_t
#include <stdlib.h> // NULL
#include <string.h> // memcpy
#include <zone.h> // Zone::Allocate
#include <assert.h> // assert

namespace candor {
namespace internal {

Heap* Heap::current_ = NULL;

Space::Space(Heap* heap, uint32_t page_size) : heap_(heap),
                                               page_size_(page_size),
                                               size_(0) {
  // Create the first page
  pages_.Push(new Page(page_size));
  pages_.allocated = true;

  select(pages_.head()->value());
}


void Space::select(Page* page) {
  top_ = &page->top_;
  limit_ = &page->limit_;
}


void Space::AddPage(uint32_t size) {
  uint32_t real_size = RoundUp(size, page_size());
  Page* page = new Page(real_size);
  pages_.Push(page);
  size_ += real_size;

  select(page);
}


char* Space::Allocate(uint32_t bytes) {
  // If current page was exhausted - run GC
  uint32_t even_bytes = bytes + (bytes & 0x01);
  bool place_in_current = *top_ + even_bytes <= *limit_;

  if (!place_in_current) {
    // Go through all pages to find gap
    List<Page*, EmptyClass>::Item* item = pages_.head();
    for (;*top_ + even_bytes > *limit_ && item != NULL; item = item->next()) {
      select(item->value());
    }

    // No gap was found - allocate new page
    if (item == NULL) {
      if (size() > size_limit()) {
        heap()->needs_gc(this == heap()->new_space() ?
            Heap::kGCNewSpace
            :
            Heap::kGCOldSpace);
      }

      AddPage(even_bytes);
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
    size_ += pages_.tail()->value()->size_;
  }

  select(pages_.head()->value());
  compute_size_limit();
}


void Space::Clear() {
  size_ = 0;
  while (pages_.length() != 0) {
    delete pages_.Shift();
  }
}


char* Heap::AllocateTagged(HeapTag tag, TenureType tenure, uint32_t bytes) {
  char* result = space(tenure)->Allocate(bytes + 8);
  uint64_t qtag = tag;
  if (tenure == kTenureOld) {
    qtag = qtag | (kMinOldSpaceGeneration << 8);
  }
  *reinterpret_cast<uint64_t*>(result) = qtag;

  return result;
}


void Heap::Reference(HValue** reference, HValue* value) {
  references()->Push(new HValueReference(reference, value));
}


void Heap::Dereference(HValue** reference, HValue* value) {
  HValueRefList::Item* tail = references()->tail();
  while (tail != NULL) {
    if (tail->value()->reference() == reference &&
        tail->value()->value() == value) {
      references()->Remove(tail);
      break;
    }
    tail = tail->prev();
  }
}


void Heap::AddWeak(HValue* value, WeakCallback callback) {
  weak_references()->Push(new HValueWeakRef(value, callback));
}


void Heap::RemoveWeak(HValue* value) {
  HValueWeakRefList::Item* tail = weak_references()->tail();
  while (tail != NULL) {
    if (tail->value()->value() == value) {
      weak_references()->Remove(tail);
    }
  }
}


HValue* HValue::CopyTo(Space* old_space, Space* new_space) {
  assert(!IsUnboxed(addr()));

  uint32_t size = 8;
  switch (tag()) {
   case Heap::kTagContext:
    // parent + slots
    size += 16 + As<HContext>()->slots() * 8;
    break;
   case Heap::kTagFunction:
    // parent + body
    size += 24;
    break;
   case Heap::kTagNumber:
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
   case Heap::kTagArray:
    // mask + map + length
    size += 24;
    break;
   case Heap::kTagMap:
    // size + space ( keys + values )
    size += 8 + (As<HMap>()->size() << 4);
    break;
   case Heap::kTagCData:
    // size + data
    size += 8 + As<HCData>()->size();
    break;
   default:
    UNEXPECTED
  }

  IncrementGeneration();
  char* result;
  if (Generation() >= Heap::kMinOldSpaceGeneration) {
    result = old_space->Allocate(size);
  } else {
    result = new_space->Allocate(size);
  }

  memcpy(result, addr(), size);

  return HValue::Cast(result);
}


char* HContext::New(Heap* heap,
                    List<char*, ZoneObject>* values) {
  char* result = heap->AllocateTagged(Heap::kTagContext,
                                      Heap::kTenureOld,
                                      16 + values->length() * 8);

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


char* HNumber::New(Heap* heap, int64_t value) {
  return reinterpret_cast<char*>(Tag(value));
}


char* HNumber::New(Heap* heap, Heap::TenureType tenure, double value) {
  char* result = heap->AllocateTagged(Heap::kTagNumber, tenure, 8);
  *reinterpret_cast<double*>(result + 8) = value;
  return result;
}


char* HBoolean::New(Heap* heap, Heap::TenureType tenure, bool value) {
  char* result = heap->AllocateTagged(Heap::kTagBoolean, tenure, 8);
  *reinterpret_cast<int8_t*>(result + 8) = value ? 1 : 0;

  return result;
}


char* HString::New(Heap* heap,
                   Heap::TenureType tenure,
                   uint32_t length) {
  char* result = heap->AllocateTagged(Heap::kTagString, tenure, length + 24);

  // Zero hash
  *reinterpret_cast<uint64_t*>(result + 8) = 0;
  // Set length
  *reinterpret_cast<uint64_t*>(result + 16) = length;

  return result;
}


char* HString::New(Heap* heap,
                   Heap::TenureType tenure,
                   const char* value,
                   uint32_t length) {
  char* result = New(heap, tenure, length);

  // Copy value
  memcpy(result + 24, value, length);

  return result;
}


uint32_t HString::Hash(char* addr) {
  uint32_t* hash_addr = reinterpret_cast<uint32_t*>(addr + 8);
  uint32_t hash = *hash_addr;
  if (hash == 0) {
    hash = ComputeHash(Value(addr), Length(addr));
    *hash_addr = hash;
  }
  return hash;
}


char* HObject::NewEmpty(Heap* heap) {
  uint32_t size = 16;

  char* obj = heap->AllocateTagged(Heap::kTagObject, Heap::kTenureNew, 16);
  char* map = heap->AllocateTagged(Heap::kTagMap,
                                   Heap::kTenureNew,
                                   (size << 4) + 8);

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


char* HArray::NewEmpty(Heap* heap) {
  uint32_t size = 16;

  char* obj = heap->AllocateTagged(Heap::kTagArray, Heap::kTenureNew, 24);
  char* map = heap->AllocateTagged(Heap::kTagMap,
                                   Heap::kTenureNew,
                                   (size << 4) + 8);

  // Set mask
  *reinterpret_cast<uint64_t*>(obj + 8) = (size - 1) << 3;
  // Set map
  *reinterpret_cast<char**>(obj + 16) = map;

  // Set length
  SetLength(obj, 0);

  // Set map's size
  *reinterpret_cast<uint64_t*>(map + 8) = size;

  // Nullify all map's slots (both keys and values)
  memset(map + 16, 0, size << 4);

  return obj;
}

int64_t HArray::Length(char* obj, bool shrink) {
  int64_t result = *reinterpret_cast<int64_t*>(obj + 24);

  if (shrink) {
    // Lookup property at [length - 1]
    // Shrink if it's nil
    //
    // NOTE: passing NULL as heap is completely safe here,
    // as we ain't going to allocate or change anything
    int64_t shrinked = result;
    char* shrinkedptr;
    char** slot;
    do {
      if (shrinked < 0) break;
      shrinked--;
      shrinkedptr = reinterpret_cast<char*>(HNumber::Tag(shrinked));
      slot = reinterpret_cast<char**>(RuntimeLookupProperty(NULL,
                                                            obj,
                                                            shrinkedptr,
                                                            0));
    } while (*slot == NULL);

    // If array was shrinked - change length
    if (result != (shrinked - 1)) {
      result = shrinked + 1;
      SetLength(obj, result);
    }
  }

  return result;
}


char* HFunction::New(Heap* heap, char* parent, char* addr, char* root) {
  char* fn = heap->AllocateTagged(Heap::kTagFunction, Heap::kTenureOld, 24);

  // Set parent context
  *reinterpret_cast<char**>(fn + 8) = parent;

  // Set pointer to code
  *reinterpret_cast<char**>(fn + 16) = addr;

  // Set root context
  *reinterpret_cast<char**>(fn + 24) = root;

  return fn;
}


char* HFunction::NewBinding(Heap* heap, char* addr, char* root) {
  // TODO: Use const here
  return New(heap, reinterpret_cast<char*>(0x0DEF0DEF), addr, root);
}


char* HCData::New(Heap* heap, size_t size) {
  char* d = heap->AllocateTagged(Heap::kTagCData, Heap::kTenureNew, 8 + size);

  *reinterpret_cast<uint32_t*>(d + 8) = size;

  return d;
}

} // namespace internal
} // namespace candor
