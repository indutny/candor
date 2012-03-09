#include "gc.h"
#include "heap.h"
#include "heap-inl.h"

#include <sys/types.h> // off_t
#include <stdlib.h> // NULL
#include <assert.h> // assert

namespace candor {
namespace internal {

void GC::GCValue::Relocate(char* address) {
  if (slot_ != NULL) {
    *slot_ = address;
  }
  if (!value()->IsGCMarked()) value()->SetGCMark(address);
}

void GC::CollectGarbage(char* stack_top) {
  assert(grey_items()->length() == 0);

  // __$gc() isn't setting needs_gc() attribute
  if (heap()->needs_gc() == Heap::kGCNone) {
    heap()->needs_gc(Heap::kGCNewSpace);
  }

  // Select space to GC
  Space* space = heap()->needs_gc() == Heap::kGCNewSpace ?
      heap()->new_space()
      :
      heap()->old_space();

  // Temporary space which will contain copies of all visited objects
  Space tmp_space(heap(), space->page_size());

  // Add referenced in C++ land values to the grey list
  HValueRefList::Item* item = heap()->references()->head();
  while (item != NULL) {
    HValueReference* ref = item->value();
    grey_items()->Push(
        new GCValue(ref->value(), reinterpret_cast<char**>(ref->reference())));
    grey_items()->Push(
        new GCValue(ref->value(), reinterpret_cast<char**>(ref->valueptr())));

    item = item->next();
  }

  // Go through the stack
  char** top = reinterpret_cast<char**>(stack_top);
  for (; top != NULL; top++) {
    // Once found enter frame signature
    // skip stack entities until last exit frame position (or NULL)
    while (top != NULL && *reinterpret_cast<uint32_t*>(top) == 0xFEEDBEEF) {
      top = *reinterpret_cast<char***>(top + 1);
    }
    if (top == NULL) break;

    // Skip rbp as well
    if (HValue::GetTag(*(top + 1)) == Heap::kTagCode) {
      top++;
      continue;
    }

    char* value = *top;

    // Skip NULL pointers, non-pointer values and rbp pushes
    if (value == NULL || HValue::IsUnboxed(value)) continue;

    // Ignore return addresses
    HValue* hvalue = HValue::Cast(value);
    if (hvalue == NULL || hvalue->tag() == Heap::kTagCode) continue;

    grey_items()->Push(new GCValue(hvalue, top));
  }

  while (grey_items()->length() != 0) {
    GCValue* value = grey_items()->Shift();

    // Skip unboxed address
    if (value->value() == NULL || HValue::IsUnboxed(value->value()->addr())) {
      continue;
    }

    if (!value->value()->IsGCMarked()) {
      // Object is in not in current space, don't move it
      if ((heap()->needs_gc() == Heap::kGCOldSpace &&
          value->value()->Generation() < Heap::kMinOldSpaceGeneration) ||
          (heap()->needs_gc() == Heap::kGCNewSpace &&
          value->value()->Generation() >= Heap::kMinOldSpaceGeneration)) {
        GC::VisitValue(value->value());
        continue;
      }

      HValue* hvalue;

      if (heap()->needs_gc() == Heap::kGCNewSpace) {
        // New space GC
        hvalue = value->value()->CopyTo(heap()->old_space(), &tmp_space);
      } else {
        // Old space GC
        hvalue = value->value()->CopyTo(&tmp_space, heap()->new_space());
      }

      value->Relocate(hvalue->addr());
      GC::VisitValue(hvalue);
    } else {
      value->Relocate(value->value()->GetGCMark());
    }
  }

  space->Swap(&tmp_space);

  // Reset GC flag
  heap()->needs_gc(Heap::kGCNone);
}


void GC::VisitValue(HValue* value) {
  switch (value->tag()) {
   case Heap::kTagContext:
    return VisitContext(value->As<HContext>());
   case Heap::kTagFunction:
    return VisitFunction(value->As<HFunction>());
   case Heap::kTagObject:
    return VisitObject(value->As<HObject>());
   case Heap::kTagMap:
    return VisitMap(value->As<HMap>());

   // String and numbers ain't referencing anyone
   case Heap::kTagString:
   case Heap::kTagNumber:
   case Heap::kTagBoolean:
    return;
   default:
    UNEXPECTED
  }
}


void GC::VisitContext(HContext* context) {
  if (context->has_parent()) {
    grey_items()->Push(
        new GCValue(HValue::Cast(context->parent()), context->parent_slot()));
  }

  for (uint32_t i = 0; i < context->slots(); i++) {
    if (!context->HasSlot(i)) continue;

    HValue* value = context->GetSlot(i);
    grey_items()->Push(new GCValue(value, context->GetSlotAddress(i)));
  }
}


void GC::VisitFunction(HFunction* fn) {
  if (fn->parent_slot() == NULL || fn->parent() == NULL) return;
  grey_items()->Push(new GCValue(HValue::Cast(fn->parent()), fn->parent_slot()));
}


void GC::VisitObject(HObject* obj) {
  grey_items()->Push(new GCValue(HValue::Cast(obj->map()), obj->map_slot()));
}


void GC::VisitMap(HMap* map) {
  for (uint32_t i = 0; i < map->size(); i++) {
    grey_items()->Push(new GCValue(map->GetSlot(i),
                                   map->GetSlotAddress(i)));
    grey_items()->Push(new GCValue(map->GetSlot(i + map->size()),
                                   map->GetSlotAddress(i + map->size())));
  }
}

} // namespace internal
} // namespace candor
