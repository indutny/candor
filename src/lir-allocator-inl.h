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


inline LIRInterval* LIRInterval::Split(int pos) {
  assert(start() < pos && pos < end());

  LIRInterval* child = new LIRInterval(pos, end());
  child->parent(this);
  end(pos);

  return child;
}

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_ALLOCATOR_INL_H_
