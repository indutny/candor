#ifndef _SRC_LIR_INL_H_
#define _SRC_LIR_INL_H_

#include "lir.h"

namespace candor {
namespace internal {
namespace lir {

inline int LGen::block_id() {
  return block_id_++;
}


inline int LGen::instr_id() {
  return instr_id_++;
}


inline int LRange::start() {
  return start_;
}


inline int LRange::end() {
  return end_;
}


inline LInstruction* LUse::instr() {
  return instr_;
}


inline bool LOperand::is_unallocated() {
  return type_ == kUnallocated;
}


inline bool LOperand::is_register() {
  return type_ == kRegister;
}


inline bool LOperand::is_stackslot() {
  return type_ == kStackSlot;
}


inline int LOperand::index() {
  return index_;
}

} // namespace lir
} // namespace internal
} // namespace candor

#endif // _SRC_LIR_INL_H_
