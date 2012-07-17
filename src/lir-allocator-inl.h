#ifndef _SRC_LIR_ALLOCATOR_INL_H_
#define _SRC_LIR_ALLOCATOR_INL_H_

#include "utils.h" // PrintBuffer
#include "macroassembler.h"
#include "hir.h"

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
  if (interval == NULL || interval->enumerated()) return;
  interval->enumerated(true);

  unhandled()->InsertSorted<LIRIntervalShape>(interval);
}


inline void LIRAllocator::AssignSpill(LIRInterval* interval) {
  LIROperand* operand;

  // Fixed intervals can't be spilled!
  assert(!interval->is_fixed());

  if (available_spills()->length() != 0) {
    operand = available_spills()->Pop();
  } else {
    operand = new LIROperand(LIROperand::kSpill, spill_count());
    spill_count_++;
  }

  interval->operand(operand);
}


inline void LIRAllocator::ReleaseSpill(LIROperand* spill) {
  assert(spill->is_spill());
  LIROperandList::Item* item;
#ifndef NDEBUG
  // No repeating items!
  for (item = available_spills()->head(); item != NULL; item = item->next()) {
    assert(!item->value()->is_equal(spill));
  }
#endif
  available_spills()->Push(spill);
}

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_ALLOCATOR_INL_H_
