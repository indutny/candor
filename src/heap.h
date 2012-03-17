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
#include <sys/types.h> // size_t

namespace candor {
namespace internal {

// Forward declarations
class Heap;
class HValueReference;
class HValueWeakRef;

class Space {
 public:
  class Page {
   public:
    Page(uint32_t size) : size_(size) {
      data_ = new char[size];
      // Make all offsets odd (pointers are tagged with 1 at last bit)
      top_ = data_ + 1;
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

  inline uint32_t size() { return size_; }
  inline uint32_t size_limit() { return size_limit_; }
  inline void compute_size_limit() {
    size_limit_ = size_ << 1;
  }

 protected:
  Heap* heap_;

  char** top_;
  char** limit_;

  inline void select(Page* page);

  List<Page*, EmptyClass> pages_;
  uint32_t page_size_;

  uint32_t size_;
  uint32_t size_limit_;
};

typedef List<HValueReference*, EmptyClass> HValueRefList;
typedef List<HValueWeakRef*, EmptyClass> HValueWeakRefList;

class Heap {
 public:
  enum HeapTag {
    kTagNil = 0x01,
    kTagContext,
    kTagBoolean,
    kTagNumber,
    kTagString,
    kTagObject,
    kTagArray,
    kTagFunction,
    kTagCData,

    kTagMap,

    // For GC (return addresses on stack will point to the JIT code
    kTagCode = 0x90
  };

  enum TenureType {
    kTenureNew = 0,
    kTenureOld = 1
  };

  enum GCType {
    kGCNone = 0,
    kGCNewSpace = 1,
    kGCOldSpace = 2
  };

  enum Error {
    kErrorNone,
    kErrorIncorrectLhs,
    kErrorCallWithoutVariable
  };

  // Positions in root register
  // NOTE: order of type strings should be the same as in HeapTag enum
  enum RootPositions {
    kRootGlobalIndex = 0,
    kRootTrueIndex = 1,
    kRootFalseIndex = 2,
    kRootNilTypeIndex = 3,
    kRootBooleanTypeIndex = 4,
    kRootNumberTypeIndex = 5,
    kRootStringTypeIndex = 6,
    kRootObjectTypeIndex = 7,
    kRootArrayTypeIndex = 8,
    kRootFunctionTypeIndex = 9,
    kRootCDataTypeIndex = 10
  };

  enum ReferenceType {
    kRefNormal,
    kRefPersistent
  };

  // Tenure configuration (GC)
  static const int8_t kMinOldSpaceGeneration = 5;
  static const uint32_t kBindingContextTag = 0x0DEC0DEC;
  static const uint32_t kEnterFrameTag = 0xFEEDBEEE;

  Heap(uint32_t page_size) : new_space_(this, page_size),
                             old_space_(this, page_size),
                             last_stack_(NULL),
                             pending_exception_(NULL),
                             needs_gc_(kGCNone),
                             gc_(this) {
    current_ = this;
    references_.allocated = true;
    reloc_references_.allocated = true;
    weak_references_.allocated = true;
  }

  // TODO: Use thread id
  static inline Heap* Current() { return current_; }

  static const char* ErrorToString(Error err);

  char* AllocateTagged(HeapTag tag, TenureType type, uint32_t bytes);

  // Referencing C++ handles
  void Reference(ReferenceType type, HValue** reference, HValue* value);
  void Dereference(HValue** reference, HValue* value);

  // Weakening C++ handles
  typedef void (*WeakCallback)(HValue* value);

  void AddWeak(HValue* value, WeakCallback callback);
  void RemoveWeak(HValue* value);

  inline Space* new_space() { return &new_space_; }
  inline Space* old_space() { return &old_space_; }

  inline Space* space(TenureType type) {
    if (type == kTenureOld) {
      return &old_space_;
    } else {
      return &new_space_;
    }
  }

  inline char** last_stack() { return &last_stack_; }
  inline char** pending_exception() { return &pending_exception_; }

  inline GCType needs_gc() { return static_cast<GCType>(needs_gc_); }
  inline GCType* needs_gc_addr() {
    return reinterpret_cast<GCType*>(&needs_gc_);
  }
  inline void needs_gc(GCType value) { needs_gc_ = value; }
  inline HValueRefList* references() { return &references_; }
  inline HValueRefList* reloc_references() { return &reloc_references_; }
  inline HValueWeakRefList* weak_references() { return &weak_references_; }

  inline GC* gc() { return &gc_; }

 private:
  Space new_space_;
  Space old_space_;

  // Support reentering candor after invoking C++ side
  char* last_stack_;

  char* pending_exception_;

  uint64_t needs_gc_;

  HValueRefList references_;
  HValueRefList reloc_references_;
  HValueWeakRefList weak_references_;

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

  HValue* CopyTo(Space* old_space, Space* new_space);

  inline bool IsGCMarked();
  inline char* GetGCMark();
  inline void SetGCMark(char* new_addr);

  inline bool IsSoftGCMarked();
  inline void SetSoftGCMark();
  inline void ResetSoftGCMark();

  inline void IncrementGeneration();
  inline uint8_t Generation();

  static inline Heap::HeapTag GetTag(char* addr);
  static inline bool IsUnboxed(char* addr);

  inline Heap::HeapTag tag() { return GetTag(addr()); }
  inline char* addr() { return reinterpret_cast<char*>(this); }
};


class HValueReference {
 public:
  HValueReference(Heap::ReferenceType type, HValue** reference, HValue* value) :
    type_(type), reference_(reference), value_(value) {
  }

  inline Heap::ReferenceType type() { return type_; }
  inline HValue** reference() { return reference_; }
  inline HValue* value() { return value_; }
  inline HValue** valueptr() { return &value_; }

  inline bool is_normal() { return type() == Heap::kRefNormal; }
  inline bool is_persistent() { return type() == Heap::kRefPersistent; }

 private:
  Heap::ReferenceType type_;
  HValue** reference_;
  HValue* value_;
};


class HValueWeakRef {
 public:
  HValueWeakRef(HValue* value, Heap::WeakCallback callback) :
      value_(value), callback_(callback) {
  }

  inline HValue* value() { return value_; }
  inline void value(HValue* value) { value_ = value; }
  inline Heap::WeakCallback callback() { return callback_; }

 private:
  HValue* value_;
  Heap::WeakCallback callback_;
};


class HNil : public HValue {
 public:
  static inline char* New() {
    return reinterpret_cast<char*>(Heap::kTagNil);
  }

  static const Heap::HeapTag class_tag = Heap::kTagNil;
};


class HContext : public HValue {
 public:
  static char* New(Heap* heap,
                   List<char*, ZoneObject>* values);

  inline bool HasSlot(uint32_t index);
  inline HValue* GetSlot(uint32_t index);
  inline char** GetSlotAddress(uint32_t index);
  static inline uint32_t GetIndexDisp(uint32_t index);

  inline char* parent() { return *parent_slot(); }
  inline bool has_parent() { return parent() != NULL; }

  inline char** parent_slot() { return reinterpret_cast<char**>(addr() + 8); }
  inline uint32_t slots() { return *reinterpret_cast<uint64_t*>(addr() + 16); }

  static const Heap::HeapTag class_tag = Heap::kTagContext;
};


class HNumber : public HValue {
 public:
  static char* New(Heap* heap, int64_t value);
  static char* New(Heap* heap, Heap::TenureType tenure, double value);

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
  static char* New(Heap* heap, Heap::TenureType tenure, bool value);

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
                   Heap::TenureType tenure,
                   uint32_t length);
  static char* New(Heap* heap,
                   Heap::TenureType tenure,
                   const char* value,
                   uint32_t length);

  inline const char* value() { return Value(addr()); }
  inline uint32_t length() { return Length(addr()); }
  inline uint32_t hash() { return Hash(addr()); }

  static uint32_t Hash(char* addr);
  static inline char* Value(char* addr) { return addr + 24; }

  static inline uint32_t Length(char* addr) {
    return *reinterpret_cast<uint32_t*>(addr + 16);
  }

  static const Heap::HeapTag class_tag = Heap::kTagString;
};


class HObject : public HValue {
 public:
  static char* NewEmpty(Heap* heap);

  inline char* map() { return *map_slot(); }
  inline char** map_slot() { return MapSlot(addr()); }
  inline uint32_t mask() { return *mask_slot(); }
  inline uint32_t* mask_slot() { return MaskSlot(addr()); }

  static inline char** MapSlot(char* addr) {
    return reinterpret_cast<char**>(addr + map_offset);
  }
  static inline char* Map(char* addr) { return *MapSlot(addr); }
  static inline uint32_t* MaskSlot(char* addr) {
    return reinterpret_cast<uint32_t*>(addr + 8);
  }
  static inline uint32_t Mask(char* addr) { return *MaskSlot(addr); }

  static char** LookupProperty(Heap* heap, char* addr, char* key, int insert);

  static const int map_offset = 16;

  static const Heap::HeapTag class_tag = Heap::kTagObject;
};


class HArray : public HObject {
 public:
  static char* NewEmpty(Heap* heap);

  static int64_t Length(char* obj, bool shrink);

  static inline void SetLength(char* obj, int64_t length) {
    *reinterpret_cast<int64_t*>(obj + 24) = length;
  }

  static const Heap::HeapTag class_tag = Heap::kTagArray;
};


class HMap : public HValue {
 public:
  inline bool IsEmptySlot(uint32_t index);
  inline HValue* GetSlot(uint32_t index);
  inline char** GetSlotAddress(uint32_t index);

  inline uint32_t size() { return *reinterpret_cast<uint32_t*>(addr() + 8); }
  inline char* space() { return addr() + space_offset; }

  static const int space_offset = 16;

  static const Heap::HeapTag class_tag = Heap::kTagMap;
};


class HFunction : public HValue {
 public:
  static char* New(Heap* heap, char* parent, char* addr, char* root);
  static char* NewBinding(Heap* heap, char* addr, char* root);

  static inline char* Root(char* addr) {
    return *reinterpret_cast<char**>(addr + 24);
  }
  static inline char* Code(char* addr) {
    return *reinterpret_cast<char**>(addr + 16);
  }
  static inline char* Parent(char* addr) {
    return *reinterpret_cast<char**>(addr + 8);
  }

  static inline char* GetContext(char* addr);
  static inline void SetContext(char* addr, char* context);

  inline char* root() { return *root_slot(); }
  inline char** root_slot() { return reinterpret_cast<char**>(addr() + 24); }
  inline char* parent() { return *parent_slot(); }
  inline char** parent_slot() { return reinterpret_cast<char**>(addr() + 8); }

  static const Heap::HeapTag class_tag = Heap::kTagFunction;
};


class HCData : public HValue {
 public:
  static char* New(Heap* heap, size_t size);

  static inline uint32_t Size(char* addr) {
    return *reinterpret_cast<uint32_t*>(addr + 8);
  }

  static inline void* Data(char* addr) {
    return reinterpret_cast<void*>(addr + 16);
  }

  inline uint32_t size() { return Size(addr()); }
  inline void* data() { return Data(addr()); }

  static const Heap::HeapTag class_tag = Heap::kTagCData;
};

} // namespace internal
} // namespace candor

#endif // _SRC_HEAP_H_
