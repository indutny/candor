#ifndef _SRC_GC_H_
#define _SRC_GC_H_

#include "zone.h" // ZoneObject
#include "utils.h" // List

namespace candor {
namespace internal {

// Forward declarations
class Heap;
class Space;
class HValue;
class HContext;
class HFunction;
class HObject;
class HArray;
class HMap;

class GC {
 public:
  class GCValue : public ZoneObject {
   public:
    GCValue(HValue* value, char** slot) : value_(value), slot_(slot) {
    }

    void Relocate(char* address);

    inline HValue* value() { return value_; }

   protected:
    HValue* value_;
    char** slot_;
  };

  enum GCType {
    kNone,
    kOldSpace,
    kNewSpace
  };

  typedef ZoneList<GCValue*> GCList;

  GC(Heap* heap) : heap_(heap), gc_type_(kNone) {
  }

  void CollectGarbage(char* stack_top);

  void ColourPersistentHandles();
  void RelocateWeakHandles();

  void ColourFrames(char* stack_top);
  void HandleWeakReferences();

  void ProcessGrey();

  void VisitValue(HValue* value);
  void VisitContext(HContext* context);
  void VisitFunction(HFunction* fn);
  void VisitObject(HObject* obj);
  void VisitArray(HArray* arr);
  void VisitMap(HMap* map);
  void VisitString(HValue* value);

  bool IsInCurrentSpace(HValue* value);

  inline void push_grey(HValue* value, char** reference) {
    grey_items()->Push(new GCValue(value, reference));
  }

  inline void push_weak(HValue* value, char** reference) {
    weak_items()->Push(new GCValue(value, reference));
  }

  inline GCList* grey_items() { return &grey_items_; }
  inline GCList* weak_items() { return &weak_items_; }
  inline GCList* black_items() { return &black_items_; }
  inline Heap* heap() { return heap_; }
  inline void tmp_space(Space* space) { tmp_space_ = space; }
  inline Space* tmp_space() { return tmp_space_; }

  inline GCType gc_type() { return gc_type_; }
  inline void gc_type(GCType value) { gc_type_ = value; }

 protected:
  GCList grey_items_;
  GCList weak_items_;
  GCList black_items_;
  Heap* heap_;
  Space* tmp_space_;

  GCType gc_type_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_GC_H_
