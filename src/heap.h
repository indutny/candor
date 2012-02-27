#ifndef _SRC_HEAP_H_
#define _SRC_HEAP_H_

//
// Heap is split into two parts:
//
//  * new space - all objects will be allocated here
//  * old space - tenured objects will be placed here
//
// Both spaces are lists of allocated buffers(pages) with a stack structure
//

#include "zone.h" // ZoneObject
#include "gc.h" // GC
#include "utils.h"

#include <stdint.h> // uint32_t

namespace dotlang {

// Forward declarations
class Heap;

class Space {
 public:
  class Page {
   public:
    Page(uint32_t size) {
      data_ = new char[size];
      top_ = data_;
      limit_ = data_ + size;
    }
    ~Page() {
      delete[] data_;
    }

    char* data_;
    char* top_;
    char* limit_;
  };

  Space(Heap* heap, uint32_t page_size);

  // Move to next page where are at least `bytes` free
  // Otherwise allocate new page
  char* Allocate(uint32_t bytes, char* context);

  inline Heap* heap() { return heap_; }

  // Both top and limit are always pointing to current page's
  // top and limit.
  inline char*** top() { return &top_; }
  inline char*** limit() { return &limit_; }

 private:
  Heap* heap_;

  char** top_;
  char** limit_;

  inline void select(Page* page);

  List<Page*, EmptyClass> pages_;
  uint32_t page_size_;
};

class Heap {
 public:
  enum HeapTag {
    kTagNil,
    kTagFunction,
    kTagContext,
    kTagNumber,
    kTagString,
    kTagObject,
    kTagMap
  };

  enum Error {
    kErrorNone,
    kErrorIncorrectLhs,
    kErrorCallWithoutVariable,
    kErrorCallNonFunction,
    kErrorNilPropertyLookup,
    kErrorNonObjectPropertyLookup
  };

  Heap(uint32_t page_size) : new_space_(this, page_size),
                             old_space_(this, page_size),
                             root_stack_(NULL),
                             pending_exception_(NULL) {
    current_ = this;
  }

  // TODO: Use thread id
  static inline Heap* Current() { return current_; }

  char* AllocateTagged(HeapTag tag, uint32_t bytes, char* context);

  inline Space* new_space() { return &new_space_; }
  inline Space* old_space() { return &old_space_; }
  inline char** root_stack() { return &root_stack_; }
  inline char** pending_exception() { return &pending_exception_; }

  inline GC* gc() { return &gc_; }

 private:
  Space new_space_;
  Space old_space_;

  // Runtime exception support
  // root stack address is needed to unwind stack up to root function's entry
  char* root_stack_;
  char* pending_exception_;

  GC gc_;

  static Heap* current_;
};


class HValue : public ZoneObject {
 public:
  HValue(Heap* heap, char* addr);
  HValue(char* addr);

  template <class T>
  static inline T* As(Heap* heap, char* addr) {
    assert(addr != NULL);
    assert(*reinterpret_cast<uint8_t*>(addr) == T::class_tag);
    return new T(heap, addr);
  }

  template <class T>
  static inline T* As(char* addr) {
    assert(addr != NULL);
    assert(*reinterpret_cast<uint8_t*>(addr) == T::class_tag);
    return new T(addr);
  }

  static Heap::HeapTag GetTag(char* addr);

  inline Heap::HeapTag tag() { return tag_; }
  inline void tag(Heap::HeapTag tag) { tag_ = tag; }

  inline char* addr() { return addr_; }
  inline void addr(char* addr) { addr_ = addr; }

  inline Heap* heap() { return heap_; }

 protected:
  Heap::HeapTag tag_;
  char* addr_;
  Heap* heap_;
};


class HContext : public HValue {
 public:
  HContext(char* addr);

  inline uint32_t slots() { return slots_; }

  static const Heap::HeapTag class_tag = Heap::kTagContext;

 protected:
  uint32_t slots_;
};


class HNumber : public HValue {
 public:
  HNumber(char* addr);

  inline int64_t value() { return value_; }

  static const Heap::HeapTag class_tag = Heap::kTagNumber;

 protected:
  int64_t value_;
};


class HString : public HValue {
 public:
  HString(char* addr);

  inline char* value() { return value_; }
  inline uint32_t length() { return length_; }
  inline uint32_t hash() { return hash_; }

  static const Heap::HeapTag class_tag = Heap::kTagString;

 protected:
  char* value_;
  uint32_t length_;
  uint32_t hash_;
};


class HObject : public HValue {
 public:
  HObject(Heap* heap, char* addr);

  static const Heap::HeapTag class_tag = Heap::kTagObject;
};


class HFunction : public HValue {
 public:
  HFunction(char* addr);

  static const Heap::HeapTag class_tag = Heap::kTagFunction;
};


} // namespace dotlang

#endif // _SRC_HEAP_H_
