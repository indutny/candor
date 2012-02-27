#include "gc.h"
#include "heap.h"

#include <assert.h> // assert

namespace dotlang {

void GC::GCValue::Relocate(char* address) {
  if (slot_ != NULL) {
    *slot_ = address;
    value()->SetGCMark(address);
  }
}

void GC::CollectGarbage(char* stack_top) {
  assert(grey_items()->length() == 0);

  // Temporary space which will contain copies of all visited objects
  Space space(heap(), heap()->new_space()->page_size());

  // Go through the stack, down to the root_stack() address

  // Visit all grey items as list grows
  GCList::Item* item = grey_items()->head();
  while (item != NULL) {
    GCValue* value = item->value();

    if (!value->value()->IsGCMarked()) {
      // Every visiting function returns new address of object
      value->Relocate(GC::VisitValue(value->value()));
    }

    item = item->next();
  }

  // Remove marks on finish
  while (grey_items()->length() != 0) {
    GCValue* value = grey_items()->Shift();
    value->value()->SetGCMark(false);
  }
}


char* GC::VisitValue(HValue* value) {
  switch (value->tag()) {
   case Heap::kTagContext:
    return VisitContext(value->As<HContext>());
   default:
    assert(0 && "Not implemented yet");
  }
}


char* GC::VisitContext(HContext* context) {
  for (uint32_t i = 0; i < context->slots(); i++) {
    HValue* value = context->GetSlot(i);
    grey_items()->Push(new GCValue(value, context->GetSlotAddress(i)));
  }

  return NULL;
}

} // namespace dotlang
