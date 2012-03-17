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

  typedef List<GCValue*, ZoneObject> GCList;

  GC(Heap* heap) : heap_(heap) {
  }

  void CollectGarbage(char* current_frame);

  void ColourPersistentHandles();
  void RelocateNormalHandles();

  void ColourFrames(char* current_frame);
  void HandleWeakReferences();

  void ProcessGrey();

  void VisitValue(HValue* value);
  void VisitContext(HContext* context);
  void VisitFunction(HFunction* fn);
  void VisitObject(HObject* obj);
  void VisitArray(HArray* arr);
  void VisitMap(HMap* map);

  bool IsInCurrentSpace(HValue* value);

  inline void push_grey(HValue* value, char** reference) {
    grey_items()->Push(new GCValue(value, reference));
  }

  inline GCList* grey_items() { return &grey_items_; }
  inline GCList* black_items() { return &black_items_; }
  inline Heap* heap() { return heap_; }
  inline void tmp_space(Space* space) { tmp_space_ = space; }
  inline Space* tmp_space() { return tmp_space_; }

 protected:
  GCList grey_items_;
  GCList black_items_;
  Heap* heap_;
  Space* tmp_space_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_GC_H_
