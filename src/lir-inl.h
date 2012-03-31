#ifndef _SRC_LIR_INL_H_
#define _SRC_LIR_INL_H_

#include "hir.h"

#if CANDOR_ARCH_x64
#include "x64/lir-instructions-x64.h"
#elif CANDOR_ARCH_ia32
#include "ia32/lir-instructions-ia32.h"
#endif

#include <sys/types.h> // off_t

namespace candor {
namespace internal {

inline LIRInstruction* LIR::Cast(HIRInstruction* instr) {
#define LIR_GEN_CAST(V)\
  case HIRInstruction::k##V:\
    result = new LIR##V();\
    result->hir(instr);\
    result->id(instr->id());\
    break;

  LIRInstruction* result = NULL;
  switch (instr->type()) {
   LIR_ENUM_INSTRUCTIONS(LIR_GEN_CAST)
   default: UNEXPECTED break;
  }

  return result;
#undef LIR_GEN_CAST
}


} // namespace internal
} // namespace candor

#endif // _SRC_LIR_INL_H_
