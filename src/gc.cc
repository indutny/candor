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


void GC::CollectGarbage(char* current_frame) {
  assert(grey_items()->length() == 0);
  assert(black_items()->length() == 0);

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
  ColourPersistentHandles();

  // Colour on-stack registers
  ColourFrames(current_frame);

  while (grey_items()->length() != 0) {
    GCValue* value = grey_items()->Shift();

    // Skip unboxed address
    if (value->value() == HValue::Cast(HNil::New()) ||
        HValue::IsUnboxed(value->value()->addr())) {
      continue;
    }

    if (!value->value()->IsGCMarked()) {
      // Object is in not in current space, don't move it
      if (!IsInCurrentSpace(value->value())) {
        if (!value->value()->IsSoftGCMarked()) {
          // Set soft mark and add item to black list to reset mark later
          value->value()->SetSoftGCMark();
          black_items()->Push(value);

          GC::VisitValue(value->value());
        }
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

  // Reset marks for items from external space
  while (black_items()->length() != 0) {
    GCValue* value = black_items()->Shift();
    assert(value->value()->IsSoftGCMarked());
    value->value()->ResetSoftGCMark();
  }

  RelocateNormalHandles();

  // Visit all weak references and call callbacks if some of them are dead
  HandleWeakReferences();

  space->Swap(&tmp_space);

  // Reset GC flag
  heap()->needs_gc(Heap::kGCNone);
}


void GC::ColourPersistentHandles() {
  HValueRefList::Item* item = heap()->references()->head();
  while (item != NULL) {
    HValueReference* ref = item->value();
    if (ref->is_persistent()) {
      grey_items()->Push(
          new GCValue(ref->value(), reinterpret_cast<char**>(ref->reference())));
      grey_items()->Push(
          new GCValue(ref->value(), reinterpret_cast<char**>(ref->valueptr())));
    }

    item = item->next();
  }
}


void GC::RelocateNormalHandles() {
  HValueRefList::Item* item = heap()->references()->head();
  while (item != NULL) {
    HValueReference* ref = item->value();
    if (ref->is_normal()) {
      GCValue* v;
      v = new GCValue(ref->value(), reinterpret_cast<char**>(ref->reference()));
      if (v->value()->IsGCMarked()) v->Relocate(v->value()->GetGCMark());

      v = new GCValue(ref->value(), reinterpret_cast<char**>(ref->valueptr()));
      if (v->value()->IsGCMarked()) v->Relocate(v->value()->GetGCMark());
    }

    item = item->next();
  }
}


void GC::ColourFrames(char* current_frame) {
  // Go through the frames
  char** frame = reinterpret_cast<char**>(current_frame);
  while (true) {
    //
    // Frame layout
    // ... [previous frame] [on-stack vars (and spills) count] [...vars...]
    // or
    // [previous frame] [xFEEDBEEF] [return addr] [rbp] ....
    //
    while (frame != NULL &&
           *reinterpret_cast<uint32_t*>(frame + 2) == 0xFEEDBEEF) {
      frame = *reinterpret_cast<char***>(frame + 3);
    }
    if (frame == NULL) break;

    uint32_t slots = (*reinterpret_cast<uint32_t*>(frame - 1)) >> 3;

    for (uint32_t i = 0; i < slots; i++) {
      char* value = *(frame - 2 - i);

      // Skip nil, non-pointer values and rbp pushes
      if (value == HNil::New() || HValue::IsUnboxed(value)) {
        continue;
      }

      // Ignore return addresses
      HValue* hvalue = HValue::Cast(value);
      if (hvalue->tag() == Heap::kTagCode) continue;

      grey_items()->Push(new GCValue(hvalue, frame - 2 - i));
    }

    frame = reinterpret_cast<char**>(*frame);
  }
}


void GC::HandleWeakReferences() {
  HValueWeakRefList::Item* item = heap()->weak_references()->head();
  while (item != NULL) {
    HValueWeakRef* ref = item->value();
    if (!ref->value()->IsGCMarked()) {
      if (IsInCurrentSpace(ref->value())) {
        // Value is in GC space and wasn't marked
        // call callback as it was GCed
        ref->callback()(ref->value());
        HValueWeakRefList::Item* current = item;
        item = item->next();
        heap()->weak_references()->Remove(current);
        continue;
      }
    } else {
      // Value wasn't GCed, but was moved
      ref->value(reinterpret_cast<HValue*>(ref->value()->GetGCMark()));
    }

    item = item->next();
  }
}


bool GC::IsInCurrentSpace(HValue* value) {
  return (heap()->needs_gc() == Heap::kGCOldSpace &&
         value->Generation() >= Heap::kMinOldSpaceGeneration) ||
         (heap()->needs_gc() == Heap::kGCNewSpace &&
         value->Generation() < Heap::kMinOldSpaceGeneration);
}


void GC::VisitValue(HValue* value) {
  switch (value->tag()) {
   case Heap::kTagContext:
    return VisitContext(value->As<HContext>());
   case Heap::kTagFunction:
    return VisitFunction(value->As<HFunction>());
   case Heap::kTagObject:
    return VisitObject(value->As<HObject>());
   case Heap::kTagArray:
    return VisitArray(value->As<HArray>());
   case Heap::kTagMap:
    return VisitMap(value->As<HMap>());

   // String and numbers ain't referencing anyone
   case Heap::kTagString:
   case Heap::kTagNumber:
   case Heap::kTagBoolean:
   case Heap::kTagCData:
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
  // TODO: Use const here
  if (fn->parent_slot() != NULL &&
      fn->parent() != reinterpret_cast<char*>(0x0DEF0DEF)) {
    grey_items()->Push(new GCValue(
          HValue::Cast(fn->parent()), fn->parent_slot()));
  }
  if (fn->root_slot() != NULL) {
    grey_items()->Push(new GCValue(HValue::Cast(fn->root()), fn->root_slot()));
  }
}


void GC::VisitObject(HObject* obj) {
  grey_items()->Push(new GCValue(HValue::Cast(obj->map()), obj->map_slot()));
}


void GC::VisitArray(HArray* arr) {
  grey_items()->Push(new GCValue(HValue::Cast(arr->map()), arr->map_slot()));
}


void GC::VisitMap(HMap* map) {
  uint32_t size = map->size() << 1;
  for (uint32_t i = 0; i < size; i++) {
    if (map->GetSlot(i) != HValue::Cast(HNil::New())) {
      grey_items()->Push(new GCValue(map->GetSlot(i),
                                     map->GetSlotAddress(i)));
    }
  }
}

} // namespace internal
} // namespace candor
