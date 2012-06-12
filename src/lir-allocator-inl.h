#ifndef _SRC_LIR_ALLOCATOR_INL_H_
#define _SRC_LIR_ALLOCATOR_INL_H_

#include "utils.h" // PrintBuffer
#include "macroassembler.h"

namespace candor {
namespace internal {

inline void LIROperand::Print(PrintBuffer* p) {
  if (is_register()) {
    p->Print("%%%s", RegisterNameByIndex(value()));
  } else if (is_spill()) {
    p->Print("[%llu]", value());
  } else {
    p->Print("%llu", value());
  }
}


inline int LIRLiveRange::FindIntersection(LIRLiveRange* range) {
  if (this->end() <= range->start() || this->start() >= range->end()) return -1;

  if (this->start() < range->start()) {
    // this:  xxx
    // range:  xxx
    return range->start();
  } else {
    // this:   xxx
    // range: xxx
    return this->start();
  }
}


inline void LIRAllocator::AddUnhandled(LIRInterval* interval) {
  if (interval->enumerated()) return;
  interval->enumerated(true);

  unhandled()->InsertSorted<LIRIntervalShape>(interval);
}

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_ALLOCATOR_INL_H_
