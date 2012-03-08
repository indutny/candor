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

namespace candor {
namespace internal {

// Forward declarations
class Heap;
class HValueReference;

class Space {
 public:
  class Page {
   public:
    Page(uint32_t size) : size_(size) {
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
    uint32_t size_;
  };

  Space(Heap* heap, uint32_t page_size);

  // Returns total size of all pages
  uint32_t Size();

  // Adds empty page of specific size
  void AddPage(uint32_t size);

  // Move to next page where are at least `bytes` free
  // Otherwise allocate new page
  char* Allocate(uint32_t bytes);

  // Deallocate all pages and take all from the `space`
  void Swap(Space* space);

  // Remove all pages
  void Clear();

  inline Heap* heap() { return heap_; }

  // Both top and limit are always pointing to current page's
  // top and limit.
  inline char*** top() { return &top_; }
  inline char*** limit() { return &limit_; }

  inline uint32_t page_size() { return page_size_; }

 protected:
  Heap* heap_;

  char** top_;
  char** limit_;

  inline void select(Page* page);

  List<Page*, EmptyClass> pages_;
  uint32_t page_size_;
  uint32_t allocations_;
};

typedef List<HValueReference*, EmptyClass> HValueRefList;

class Heap {
 public:
  enum HeapTag {
    kTagNil,
    kTagFunction,
    kTagContext,
    kTagNumber,
    kTagString,
    kTagBoolean,
    kTagObject,
    kTagMap,

    // For GC (return addresses on stack will point to the JIT code
    kTagCode = 0x90
  };

  enum Error {
    kErrorNone,
    kErrorIncorrectLhs,
    kErrorCallWithoutVariable
  };

  Heap(uint32_t page_size) : new_space_(this, page_size),
                             old_space_(this, page_size),
                             last_stack_(NULL),
                             pending_exception_(NULL),
                             needs_gc_(0),
                             gc_(this) {
    current_ = this;
    references_.allocated = true;
  }

  // TODO: Use thread id
  static inline Heap* Current() { return current_; }

  char* AllocateTagged(HeapTag tag, uint32_t bytes);
  void Reference(HValue** reference, HValue* value);
  void Dereference(HValue** reference, HValue* value);

  inline Space* new_space() { return &new_space_; }
  inline Space* old_space() { return &old_space_; }
  inline char** last_stack() { return &last_stack_; }
  inline char** pending_exception() { return &pending_exception_; }

  inline uint64_t needs_gc() { return needs_gc_; }
  inline uint64_t* needs_gc_addr() { return &needs_gc_; }
  inline void needs_gc(uint64_t value) { needs_gc_ = value; }
  inline HValueRefList* references() { return &references_; }

  inline GC* gc() { return &gc_; }

 private:
  Space new_space_;
  Space old_space_;

  // Support reentering candor after invoking C++ side
  char* last_stack_;

  char* pending_exception_;

  uint64_t needs_gc_;

  HValueRefList references_;

  GC gc_;

  static Heap* current_;
};


class HValue {
 public:
  HValue() { UNEXPECTED }

  static inline HValue* Cast(char* addr) {
    return reinterpret_cast<HValue*>(addr);
  }

  template <class T>
  inline T* As() {
    assert(tag() == T::class_tag);
    return reinterpret_cast<T*>(this);
  }

  template <class T>
  static inline T* As(char* addr) {
    return Cast(addr)->As<T>();
  }

  HValue* CopyTo(Space* space);

  inline bool IsGCMarked();
  inline char* GetGCMark();
  inline void SetGCMark(char* new_addr);
  inline void ResetGCMark();

  static inline Heap::HeapTag GetTag(char* addr);
  static inline bool IsUnboxed(char* addr);

  inline Heap::HeapTag tag() { return GetTag(addr()); }
  inline char* addr() { return reinterpret_cast<char*>(this); }
};


class HValueReference {
 public:
  HValueReference(HValue** reference, HValue* value) : reference_(reference),
                                                       value_(value) {
  }

  inline HValue** reference() { return reference_; }
  inline HValue* value() { return value_; }
  inline HValue** valueptr() { return &value_; }

 private:
  HValue** reference_;
  HValue* value_;
};


class HContext : public HValue {
 public:
  static char* New(Heap* heap,
                   List<char*, ZoneObject>* values);

  bool HasSlot(uint32_t index);
  HValue* GetSlot(uint32_t index);
  char** GetSlotAddress(uint32_t index);

  inline char* parent() { return *parent_slot(); }
  inline bool has_parent() { return parent() != NULL; }

  inline char** parent_slot() { return reinterpret_cast<char**>(addr() + 8); }
  inline uint32_t slots() { return *reinterpret_cast<uint64_t*>(addr() + 16); }

  static const Heap::HeapTag class_tag = Heap::kTagContext;
};


class HNumber : public HValue {
 public:
  static char* New(Heap* heap, int64_t value);
  static char* New(Heap* heap, double value);

  static inline int64_t Untag(int64_t value);
  static inline int64_t Tag(int64_t value);

  inline double value() { return DoubleValue(addr()); }
  static inline int64_t IntegralValue(char* addr);
  static inline double DoubleValue(char* addr);

  static inline bool IsIntegral(char* addr);

  static const Heap::HeapTag class_tag = Heap::kTagNumber;
};


class HBoolean : public HValue {
 public:
  static char* New(Heap* heap, bool value);

  inline bool is_true() { return Value(addr()); }
  inline bool is_false() { return !is_true(); }

  static inline bool Value(char* addr) {
    return *reinterpret_cast<uint8_t*>(addr + 8) != 0;
  }

  static const Heap::HeapTag class_tag = Heap::kTagBoolean;
};


class HString : public HValue {
 public:
  static char* New(Heap* heap,
                   uint32_t length);
  static char* New(Heap* heap,
                   const char* value,
                   uint32_t length);

  inline const char* value() { return Value(addr()); }
  inline uint32_t length() { return Length(addr()); }
  inline uint32_t hash() { return Hash(addr()); }

  static uint32_t Hash(char* addr);
  inline static char* Value(char* addr) { return addr + 24; }

  inline static uint32_t Length(char* addr) {
    return *reinterpret_cast<uint32_t*>(addr + 16);
  }

  static const Heap::HeapTag class_tag = Heap::kTagString;
};


class HObject : public HValue {
 public:
  static char* NewEmpty(Heap* heap);

  inline char* map() { return *map_slot(); }
  inline char** map_slot() { return reinterpret_cast<char**>(addr() + 16); }
  inline uint32_t mask() { return *mask_slot(); }
  inline uint32_t* mask_slot() {
    return reinterpret_cast<uint32_t*>(addr() + 8);
  }

  static const Heap::HeapTag class_tag = Heap::kTagObject;
};


class HMap : public HValue {
 public:
  HMap(char* addr);

  bool IsEmptySlot(uint32_t index);
  HValue* GetSlot(uint32_t index);
  char** GetSlotAddress(uint32_t index);

  inline uint32_t size() { return *reinterpret_cast<uint64_t*>(addr() + 8); }
  inline char* space() { return addr() + 16; }

  static const Heap::HeapTag class_tag = Heap::kTagMap;
};


class HFunction : public HValue {
 public:
  HFunction(char* addr);
  static char* New(Heap* heap, char* parent, char* addr, char* root);
  static char* NewBinding(Heap* heap, char* addr, char* root);

  inline static char* Root(char* addr) {
    return *reinterpret_cast<char**>(addr + 24);
  }
  inline static char* Code(char* addr) {
    return *reinterpret_cast<char**>(addr + 16);
  }
  inline static char* Parent(char* addr) {
    return *reinterpret_cast<char**>(addr + 8);
  }

  inline char* parent() { return *parent_slot(); }
  inline char** parent_slot() { return reinterpret_cast<char**>(addr() + 8); }

  static const Heap::HeapTag class_tag = Heap::kTagFunction;
};

} // namespace internal
} // namespace candor

#endif // _SRC_HEAP_H_
