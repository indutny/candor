#ifndef _SRC_GC_H_
#define _SRC_GC_H_

#include "zone.h" // ZoneObject
#include "utils.h" // List

namespace dotlang {

// Forward declarations
class Heap;
class HValue;
class HContext;
class HFunction;
class HObject;
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
  typedef List<HValue*, ZoneObject> GCRawList;

  GC(Heap* heap) : heap_(heap) {
  }

  void CollectGarbage(char* stack_top);
  void VisitValue(HValue* value);
  void VisitContext(HContext* context);
  void VisitFunction(HFunction* fn);
  void VisitObject(HObject* obj);
  void VisitMap(HMap* map);

  inline GCList* grey_items() { return &grey_items_; }
  inline GCRawList* black_items() { return &black_items_; }
  inline Heap* heap() { return heap_; }

 protected:
  GCList grey_items_;
  GCRawList black_items_;
  Heap* heap_;
};

} // namespace dotlang

#endif // _SRC_GC_H_
