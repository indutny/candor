#include "pic.h"
#include "heap.h" // HObject
#include "heap-inl.h"
#include "code-space.h" // CodeSpace
#include "stubs.h" // Stubs
#include <string.h>

namespace candor {
namespace internal {

PIC::PIC(CodeSpace* space) : space_(space), index_(0) {
}


char* PIC::Generate() {
  Masm masm(space_);

  Generate(&masm);

  char* addr = space_->Put(&masm);

  // At this stage protos_ and results_ should contain offsets,
  // get real addresses for them and reference protos in heap
  for (int i = 0; i < kMaxSize; i++) {
    protos_[i] = reinterpret_cast<char**>(
        addr + reinterpret_cast<intptr_t>(protos_[i]));
    results_[i] = reinterpret_cast<intptr_t*>(
        addr + reinterpret_cast<intptr_t>(results_[i]));

    space_->heap()->Reference(Heap::kRefWeak,
                              reinterpret_cast<HValue**>(protos_[i]),
                              reinterpret_cast<HValue*>(*protos_[i]));
  }

  return addr;
}


void PIC::Miss(PIC* pic, char* object, intptr_t result) {
  pic->Miss(object, result);
}


void PIC::Miss(char* object, intptr_t result) {
  // char* on_stack;
  if (index_ >= 2 * kMaxSize) return; // pic->Invalidate(&on_stack - 2);

  Heap::HeapTag tag = HValue::GetTag(object);
  if (tag != Heap::kTagObject) return;

  char* map = HValue::As<HObject>(object)->map();
  *protos_[index_ >> 1] = HValue::As<HMap>(map)->proto();
  *results_[index_ >> 1] = result;

  // Index should look like a tagged value
  index_ += 2;
}


void PIC::Invalidate(char** ip) {
  *ip = space_->stubs()->GetLookupPropertyStub();
}

} // namespace internal
} // namespace candor
