#ifndef _SRC_LIR_INL_H_
#define _SRC_LIR_INL_H_

#include "hir.h"
#include "hir-instructions.h"

#if CANDOR_ARCH_x64
#include "x64/lir-instructions-x64.h"
#elif CANDOR_ARCH_ia32
#include "ia32/lir-instructions-ia32.h"
#endif

#include "macroassembler.h"

#include <sys/types.h> // off_t

namespace candor {
namespace internal {

inline LIRInstruction* LIR::Cast(HIRInstruction* instr) {
#define LIR_GEN_CAST(V)\
  case HIRInstruction::k##V:\
    result = new LIR##V();\
    break;

  LIRInstruction* result = NULL;
  switch (instr->type()) {
   LIR_ENUM_INSTRUCTIONS(LIR_GEN_CAST)
   default: UNEXPECTED break;
  }

  // Initialize instruction
  result->hir(instr);
  result->id(instr->id());
  result->lir(this);

  return result;
#undef LIR_GEN_CAST
}


inline void LIR::AddInstruction(LIRInstruction* instr) {
  if (last_instruction_ != NULL) {
    last_instruction_->next(instr);
    instr->prev(last_instruction_);
  } else {
    first_instruction_ = instr;
  }

  last_instruction_ = instr;
  instr->id(instruction_index());
}

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_INL_H_
