#include "pic.h"
#include "heap.h" // HObject
#include "heap-inl.h"
#include "code-space.h" // CodeSpace
#include "stubs.h" // Stubs
#include <string.h>

namespace candor {
namespace internal {

PIC::PIC(CodeSpace* space) : space_(space), index_(0) {
  memset(protos_, 0, sizeof(protos_));
  memset(results_, 0, sizeof(results_));

  for (int i = 0; i < kMaxSize; i++) {
    space->heap()->Reference(Heap::kRefWeak,
                             reinterpret_cast<HValue**>(&protos_[i]),
                             reinterpret_cast<HValue*>(protos_[i]));
  }
}


void PIC::Miss(char* object, intptr_t result) {
  char* on_stack;
  if (index_ == kMaxSize) return Invalidate(&on_stack - 2);

  Heap::HeapTag tag = HValue::GetTag(object);
  if (tag != Heap::kTagObject) return;

  char* map = HValue::As<HObject>(object)->map();
  protos_[index_ >> 1] = HValue::As<HMap>(map)->proto();
  results_[index_ >> 1] = result;

  // Index should look like a tagged value
  index_ += 2;
}


void PIC::Invalidate(char** ip) {
  *ip = space_->stubs()->GetLookupPropertyStub();
}

} // namespace internal
} // namespace candor
