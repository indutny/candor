#include "pic.h"
#include "heap.h" // HObject
#include "heap-inl.h"
#include "code-space.h" // CodeSpace
#include "stubs.h" // Stubs
#include "zone.h" // Zone
#include <string.h>

namespace candor {
namespace internal {

PIC::PIC(CodeSpace* space) : space_(space),
                             chunk_(NULL),
                             protos_(NULL),
                             results_(NULL),
                             size_(0) {
}


PIC::~PIC() {
  delete[] protos_;
  delete[] results_;
  chunk_ = NULL;
}


char* PIC::Generate() {
  Zone zone;
  Masm masm(space_);

  Generate(&masm);

  if (chunk_ != NULL) chunk_->Unref();
  chunk_ = space_->CreateChunk("__pic__", "", 0);
  space_->Put(chunk_, &masm);

  // At this stage protos_ and results_ should contain offsets,
  // get real addresses for them and reference protos in heap
  for (int i = 0; i < size_; i++) {
    proto_offsets_[i] = reinterpret_cast<char**>(
        chunk_->addr() + reinterpret_cast<intptr_t>(proto_offsets_[i]));

    space_->heap()->Reference(Heap::kRefWeak,
                              reinterpret_cast<HValue**>(proto_offsets_[i]),
                              reinterpret_cast<HValue*>(*proto_offsets_[i]));
  }

  return chunk_->addr();
}


void PIC::Miss(PIC* pic, char* object, intptr_t result, char* ip) {
  pic->Miss(object, result, ip);
}


void PIC::Miss(char* object, intptr_t result, char* ip) {
  Heap::HeapTag tag = HValue::GetTag(object);
  if (tag != Heap::kTagObject) return;

  char** call_ip = NULL;
  // Search for correct IP to replace
  for (size_t i = 3; i < 2 * sizeof(void*); i++) {
    char** iip = reinterpret_cast<char**>(ip - i);
    if (*iip == chunk_->addr()) {
      call_ip = iip;
      break;
    }
  }

  if (call_ip == NULL) return;

  // Sign extend const
  intptr_t disabled = ~Heap::kICDisabledValue;
  disabled = ~disabled;

  char* proto = HValue::As<HObject>(object)->proto();
  if ((reinterpret_cast<intptr_t>(proto) == disabled)) {
    return;
  }

  // Patch call site and remove call to PIC
  if (size_ >= kMaxSize) {
    *call_ip = space_->stubs()->GetLookupPropertyStub();
    return;
  }

  // Lazily allocate memory for protos and results
  if (size_ == 0) {
    protos_ = new char*[kMaxSize];
    results_ = new intptr_t[kMaxSize];
  }

  protos_[size_] = proto;
  results_[size_] = result;
  space_->heap()->Reference(Heap::kRefWeak,
                            reinterpret_cast<HValue**>(&protos_[size_]),
                            reinterpret_cast<HValue*>(protos_[size_]));

  // Dereference protos in previous version of PIC
  for (int i = 0; i < size_; i++) {
    space_->heap()->Dereference(reinterpret_cast<HValue**>(proto_offsets_[i]),
                                reinterpret_cast<HValue*>(*proto_offsets_[i]));
  }

  size_++;

  // Generate new PIC and replace previous one
  *call_ip = Generate();
}

} // namespace internal
} // namespace candor
