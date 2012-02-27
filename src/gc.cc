#include "gc.h"
#include "heap.h"

#include <assert.h> // assert

namespace dotlang {

void GC::CollectGarbage(HContext* context) {
  assert(grey_items()->length() == 0);
  grey_items()->Push(new GCValue(context, NULL));

  // Visit all grey items as list grows
  GCList::Item* item = grey_items()->head();
  while (item != NULL) {
    GCValue* value = item->value();

    if (!value->value()->IsGCMarked()) {
      GC::VisitValue(value->value());
      value->value()->SetGCMark(0);
    }

    item = item->next();
  }

  // Remove marks on finish
  while (grey_items()->length() != 0) {
    GCValue* value = grey_items()->Shift();
    value->value()->SetGCMark(false);
  }
}


void GC::VisitValue(HValue* value) {
  switch (value->tag()) {
   case Heap::kTagContext:
    return VisitContext(value->As<HContext>());
   default:
    assert(0 && "Not implemented yet");
  }
}


void GC::VisitContext(HContext* context) {
  for (uint32_t i = 0; i < context->slots(); i++) {
    HValue* value = context->GetSlot(i);
    grey_items()->Push(new GCValue(value, context->GetSlotAddress(i)));
  }
}

} // namespace dotlang
