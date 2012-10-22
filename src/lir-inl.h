#ifndef _SRC_LIR_INL_H_
#define _SRC_LIR_INL_H_

#include "lir.h"

namespace candor {
namespace internal {

inline int LGen::instr_id() {
  return instr_id_++;
}


inline void LGen::Print(PrintBuffer* p) {
  p->Print("lir\n");
}


inline void LGen::Print(char* out, int32_t size) {
  PrintBuffer p(out, size);
  Print(&p);
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

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_INL_H_
