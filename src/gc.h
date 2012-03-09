#ifndef _SRC_GC_H_
#define _SRC_GC_H_

#include "zone.h" // ZoneObject
#include "utils.h" // List

namespace candor {
namespace internal {

// Forward declarations
class Heap;
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

  typedef List<GCValue*, ZoneObject> GCList;

  GC(Heap* heap) : heap_(heap) {
  }

  void CollectGarbage(char* stack_top);

  void ColourHandles();
  void ColourStack(char* stack_top);
  void HandleWeakReferences();

  void VisitValue(HValue* value);
  void VisitContext(HContext* context);
  void VisitFunction(HFunction* fn);
  void VisitObject(HObject* obj);
  void VisitArray(HArray* arr);
  void VisitMap(HMap* map);

  bool IsInCurrentSpace(HValue* value);

  inline GCList* grey_items() { return &grey_items_; }
  inline GCList* black_items() { return &black_items_; }
  inline Heap* heap() { return heap_; }

 protected:
  GCList grey_items_;
  GCList black_items_;
  Heap* heap_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_GC_H_
