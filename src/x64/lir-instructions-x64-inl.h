#ifndef _SRC_LIR_INSTRUCTIONS_X64_INL_H_
#define _SRC_LIR_INSTRUCTIONS_X64_INL_H_

#include "lir-instructions-x64.h"
#include "lir.h"
#include "macroassembler.h"
#include "macroassembler-inl.h"

namespace candor {
namespace internal {

inline Register LIRInstruction::ToRegister(LIROperand* op) {
  assert(op->is_register());
  return RegisterByIndex(op->value());
}


inline Operand& LIRInstruction::ToOperand(LIROperand* op) {
  assert(op->is_spill());
  return masm()->SpillToOperand(op->value());
}

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_INSTRUCTIONS_X64_INL_H_
