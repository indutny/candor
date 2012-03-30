#include "lir.h"
#include "utils.h"

#include "hir.h"
#include "hir-instructions.h"

#if CANDOR_ARCH_x64
#include "x64/lir-instructions-x64.h"
#elif CANDOR_ARCH_ia32
#include "ia32/lir-instructions-ia32.h"
#endif

#include <stdlib.h> // NULL

namespace candor {
namespace internal {

LIR::LIR(Heap* heap, HIR* hir) : heap_(heap), hir_(hir) {
  // Visit all instructions
  HIRInstruction* hinstr = hir->first_instruction();

  for (; hinstr != NULL; hinstr = hinstr->next()) {
    LIRInstruction* linstr = Cast(hinstr);
  }
}


#define LIR_GEN_CAST(V)\
    case HIRInstruction::k##V:\
      result = new LIR##V();\
      result->hir(instr);\
      break;


LIRInstruction* LIR::Cast(HIRInstruction* instr) {
  LIRInstruction* result = NULL;
  switch (instr->type()) {
   LIR_ENUM_INSTRUCTIONS(LIR_GEN_CAST)
   default: UNEXPECTED break;
  }

  return result;
}


#undef LIR_GEN_CAST


char* LIR::Generate() {
  return NULL;
}

} // namespace internal
} // namespace candor
