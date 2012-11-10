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

  jmp_ = reinterpret_cast<uint32_t*>(
      reinterpret_cast<intptr_t>(jmp_) + addr);

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

  addr_ = addr;

  return addr;
}


void PIC::Miss(PIC* pic, char* object, intptr_t result, char* ip) {
  pic->Miss(object, result, ip);
}


void PIC::Miss(char* object, intptr_t result, char* ip) {
  // Patch call site and remove call to PIC
  if (index_ >= kMaxSize) {
    // Search for correct IP to replace
    for (size_t i = 3; i < 2 * sizeof(void*); i++) {
      char** iip = reinterpret_cast<char**>(ip - i);
      if (*iip == addr_) {
        *iip = space_->stubs()->GetLookupPropertyStub();
        break;
      }
    }
    return;
  }

  Heap::HeapTag tag = HValue::GetTag(object);
  if (tag != Heap::kTagObject) return;

  *protos_[index_] = HValue::As<HObject>(object)->proto();
  *results_[index_] = result;

  index_++;
  *jmp_ = *jmp_ - section_size_;
}

} // namespace internal
} // namespace candor
