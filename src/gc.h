#ifndef _SRC_GC_H_
#define _SRC_GC_H_

#include "zone.h" // ZoneObject
#include "utils.h" // List

namespace dotlang {

// Forward declarations
class Heap;
class HValue;
class HContext;

class GC {
 public:
  class GCValue : public ZoneObject {
   public:
    GCValue(HValue* value, char** slot) : value_(value), slot_(slot) {
    }

    inline HValue* value() { return value_; }

   protected:
    HValue* value_;
    char** slot_;
  };
  typedef List<GCValue*, ZoneObject> GCList;

  void CollectGarbage(HContext* context);
  void VisitValue(HValue* value);
  void VisitContext(HContext* context);

  inline GCList* grey_items() { return &grey_items_; }

 protected:
  GCList grey_items_;
};

} // namespace dotlang

#endif // _SRC_GC_H_
