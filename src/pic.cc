#include "pic.h"
#include "heap.h" // HObject
#include "heap-inl.h"
#include "code-space.h" // CodeSpace
#include "stubs.h" // Stubs
#include <string.h>

namespace candor {
namespace internal {

PIC::PIC(CodeSpace* space) : space_(space), index_(0) {
#ifndef NDEBUG
  memset(protos_, 0, sizeof(protos_));
  memset(props_, 0, sizeof(props_));
  memset(results_, 0, sizeof(results_));
#endif // NDEBUG
}


void PIC::Miss(char* object, char* property, intptr_t result) {
  char* on_stack;
  if (index_ == kMaxSize) return Invalidate(&on_stack - 2);

  Heap::HeapTag tag = HValue::GetTag(object);
  if (tag != Heap::kTagObject) return;

  char* map = HValue::As<HObject>(object)->map();
  protos_[index_] = HValue::As<HMap>(map)->proto();
  props_[index_] = property;
  results_[index_] = result;

  index_++;
}


void PIC::Invalidate(char** ip) {
  *ip = space_->stubs()->GetLookupPropertyStub();
}

} // namespace internal
} // namespace candor
