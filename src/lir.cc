#include "lir.h"
#include "utils.h"

#include "hir.h"
#include "hir-instructions.h"

#if CANDOR_ARCH_x64
#include "x64/lir-instructions-x64.h"
#elif CANDOR_ARCH_ia32
#include "ia32/lir-instructions-ia32.h"
#endif

#include "macroassembler.h"

#include <stdlib.h> // NULL

namespace candor {
namespace internal {

LIR::LIR(Heap* heap, HIR* hir) : heap_(heap), hir_(hir) {
  // Calculate numeric liveness ranges
  CalculateLiveness();
}


void LIR::CalculateLiveness() {
  HIRValueList::Item* item = hir()->values()->head();
  for (; item != NULL; item = item->next()) {
    HIRValue* value = item->value();

    // Skip already initialized live ranges
    if (value->live_range()->start != -1) continue;

    // Go through all instructions to determine maximum and minimum ids
    int min = 0;
    int max = 0;
    HIRInstructionList::Item* instr = value->uses()->head();
    if (instr != NULL) min = instr->value()->id();
    for (; instr != NULL; instr = instr->next()) {
      int id = instr->value()->id();
      if (id < min) min = id;
      if (id > max) max = id;
    }

    // Set liveness range
    value->live_range()->start = min;
    value->live_range()->end = max;
  }
}


LIRInstruction* LIR::Cast(HIRInstruction* instr) {
#define LIR_GEN_CAST(V)\
  case HIRInstruction::k##V:\
    result = new LIR##V();\
    result->hir(instr);\
    break;

  LIRInstruction* result = NULL;
  switch (instr->type()) {
   LIR_ENUM_INSTRUCTIONS(LIR_GEN_CAST)
   default: UNEXPECTED break;
  }

  return result;
#undef LIR_GEN_CAST
}


void LIR::Generate(Masm* masm) {
  // Visit all instructions
  HIRInstruction* hinstr = hir()->first_instruction();

  for (; hinstr != NULL; hinstr = hinstr->next()) {
    LIRInstruction* linstr = Cast(hinstr);
    linstr->masm(masm);
    linstr->Generate();
  }
}

} // namespace internal
} // namespace candor
