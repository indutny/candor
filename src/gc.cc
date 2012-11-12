#include "gc.h"
#include "heap.h"
#include "heap-inl.h"
#include "code-space.h"

#include <stdlib.h> // NULL
#include <stdint.h> // int32_t and others
#include <unistd.h> // intptr_t
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
  assert(black_items()->length() == 0);

  // __$gc() isn't setting needs_gc() attribute
  if (heap()->needs_gc() == Heap::kGCNone) {
    heap()->needs_gc(Heap::kGCNewSpace);
  }

  switch (heap()->needs_gc()) {
   case Heap::kGCNewSpace: gc_type(kNewSpace); break;
   case Heap::kGCOldSpace: gc_type(kOldSpace); break;
   default:
    UNEXPECTED
    break;
  }

  // Select space to GC
  Space* space = gc_type() == kNewSpace ?
      heap()->new_space()
      :
      heap()->old_space();

  // Temporary space which will contain copies of all visited objects
  tmp_space(new Space(heap(), space->page_size()));

  // Add referenced in C++ land values to the grey list
  ColourPersistentHandles();

  // Colour on-stack registers
  ColourFrames(stack_top);

  // Reset marks for items from external space
  while (black_items()->length() != 0) {
    GCValue* value = black_items()->Shift();
    assert(value->value()->IsSoftGCMarked());
    value->value()->ResetSoftGCMark();
  }

  RelocateWeakHandles();

  // Visit all weak references and call callbacks if some of them are dead
  HandleWeakReferences();

  space->Swap(tmp_space());
  delete tmp_space();

  if (gc_type() != kNewSpace || heap()->needs_gc() == Heap::kGCNewSpace) {
    // Reset GC flag
    heap()->needs_gc(Heap::kGCNone);
  } else {
    // Or call gc for old_space space
    CollectGarbage(stack_top);
  }
}


void GC::ColourPersistentHandles() {
  HValueRefMap::Item* item = heap()->references()->head();
  for (; item != NULL; item = item->next_scalar()) {
    HValueReference* ref = item->value();
    if (ref->is_persistent()) {
      push_grey(ref->value(), reinterpret_cast<char**>(ref->reference()));
      push_grey(ref->value(), reinterpret_cast<char**>(ref->valueptr()));
      ProcessGrey();
    }
  }
}


void GC::RelocateWeakHandles() {
  HValueRefMap::Item* item = heap()->references()->head();
  HValueRefMap::Item* next;
  for (; item != NULL; item = next) {
    HValueReference* ref = item->value();
    next = item->next_scalar();

    if (ref->is_weak()) {
      GCValue* v;

      // Skip ICs zap values and everything unboxed
      if (HValue::IsUnboxed(reinterpret_cast<char*>(ref->value()))) continue;

      if (ref->value()->IsGCMarked()) {
        v = new GCValue(ref->value(),
                        reinterpret_cast<char**>(ref->reference()));
        v->Relocate(v->value()->GetGCMark());
        v = new GCValue(ref->value(),
                        reinterpret_cast<char**>(ref->valueptr()));
        v->Relocate(v->value()->GetGCMark());
      } else {
        // Value was garbage collected - remove reference from the list
        heap()->references()->RemoveOne(item->key());
      }
    }
  }
}


void GC::ColourFrames(char* stack_top) {
  // Go through the frames
  char** frame = reinterpret_cast<char**>(stack_top);
  while (frame != NULL) {
    // Skip C++ frames
    while (frame != NULL &&
           static_cast<uint32_t>(reinterpret_cast<intptr_t>(*frame)) ==
               Heap::kEnterFrameTag) {
      frame = reinterpret_cast<char**>(*(frame + 1));
    }
    if (frame == NULL) break;

    char* value = *frame;
    // Skip nil, non-pointer values and rbp pushes
    if (value != HNil::New() && !HValue::IsUnboxed(value)) {
      push_grey(HValue::Cast(value), frame);
      ProcessGrey();
    }

    frame++;
  }
}


void GC::HandleWeakReferences() {
  HValueWeakRefMap::Item* item = heap()->weak_references()->head();
  HValueWeakRefMap::Item* next;
  for (; item != NULL; item = next) {
    HValueWeakRef* ref = item->value();
    next = item->next_scalar();

    if (!ref->value()->IsGCMarked()) {
      if (IsInCurrentSpace(ref->value())) {
        // Value is in GC space and wasn't marked
        // call callback as it was GCed
        ref->callback()(ref->value());
        heap()->weak_references()->RemoveOne(item->key());
      }
    } else {
      // Value wasn't GCed, but was moved
      ref->value(reinterpret_cast<HValue*>(ref->value()->GetGCMark()));
    }
  }

  while (weak_items()->length() != 0) {
    GCValue* value = weak_items()->Shift();

    // Skip ICs zap values and everything unboxed
    if (HValue::IsUnboxed(reinterpret_cast<char*>(value->value()))) continue;

    if (!value->value()->IsGCMarked()) {
      if (IsInCurrentSpace(value->value())) {
        // Value was GCed
        value->Relocate(NULL);
      }
    } else {
      // Value wasn't GCed, but was relocated
      value->Relocate(value->value()->GetGCMark());
    }
  }
}


void GC::ProcessGrey() {
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
      assert(!value->value()->IsSoftGCMarked());

      HValue* hvalue;

      if (gc_type() == kNewSpace) {
        // New space GC
        hvalue = value->value()->CopyTo(heap()->old_space(), tmp_space());
      } else {
        // Old space GC
        hvalue = value->value()->CopyTo(tmp_space(), heap()->new_space());
      }

      value->Relocate(hvalue->addr());
      GC::VisitValue(hvalue);
    } else {
      value->Relocate(value->value()->GetGCMark());
    }
  }
}


bool GC::IsInCurrentSpace(HValue* value) {
  return (gc_type() == kOldSpace &&
         value->Generation() >= Heap::kMinOldSpaceGeneration) ||
         (gc_type() == kNewSpace &&
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

   // non-cons strings and numbers ain't referencing anyone
   case Heap::kTagString:
    switch (HValue::GetRepresentation<HString::Representation>(value->addr())) {
     case HString::kNormal:
      break;
     case HString::kCons:
      return VisitString(value);
    }
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
    push_grey(HValue::Cast(context->parent()), context->parent_slot());
  }

  for (uint32_t i = 0; i < context->slots(); i++) {
    if (!context->HasSlot(i)) continue;

    push_grey(context->GetSlot(i), context->GetSlotAddress(i));
  }
}


void GC::VisitFunction(HFunction* fn) {
  if (fn->parent_slot() != NULL &&
      fn->parent() != reinterpret_cast<char*>(Heap::kBindingContextTag)) {
    push_grey(HValue::Cast(fn->parent()), fn->parent_slot());
  }
  if (fn->root_slot() != NULL) {
    push_grey(HValue::Cast(fn->root()), fn->root_slot());
  }
}


void GC::VisitObject(HObject* obj) {
  if (obj->proto() != NULL) {
    push_weak(HValue::Cast(obj->proto()), obj->proto_slot());
  }

  push_grey(HValue::Cast(obj->map()), obj->map_slot());
}


void GC::VisitArray(HArray* arr) {
  push_grey(HValue::Cast(arr->map()), arr->map_slot());
}


void GC::VisitMap(HMap* map) {
  uint32_t size = map->size() << 1;

  for (uint32_t i = 0; i < size; i++) {
    if (map->IsEmptySlot(i)) continue;

    push_grey(map->GetSlot(i), map->GetSlotAddress(i));
  }
}


void GC::VisitString(HValue* value) {
  push_grey(HValue::Cast(HString::LeftCons(value->addr())),
            HString::LeftConsSlot(value->addr()));
  push_grey(HValue::Cast(HString::RightCons(value->addr())),
            HString::RightConsSlot(value->addr()));
}

} // namespace internal
} // namespace candor
