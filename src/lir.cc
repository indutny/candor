#include "lir.h"
#include "lir-inl.h"
#include "utils.h"

#include "hir.h"
#include "hir-instructions.h"

#if CANDOR_ARCH_x64
#include "x64/lir-instructions-x64.h"
#elif CANDOR_ARCH_ia32
#include "ia32/lir-instructions-ia32.h"
#endif

#include "macroassembler.h"
#include "macroassembler-inl.h"
#include "source-map.h" // SourceMap

#include <stdlib.h> // NULL

namespace candor {
namespace internal {

LIR::LIR(Heap* heap, HIR* hir, Masm* masm) : allocator_(this, hir),
                                             heap_(heap),
                                             hir_(hir),
                                             masm_(masm),
                                             first_instruction_(NULL),
                                             last_instruction_(NULL) {
  BuildInstructions();
  allocator()->Init();

  Translate();
  Generate();
}


void LIR::BuildInstructions() {
  HIRInstruction* hinstr = hir()->first_instruction();

  for (; hinstr != NULL; hinstr = hinstr->next()) {
    LIRInstruction* linstr = hinstr->lir(this);
    AddInstruction(linstr);
  }
}


void LIR::Translate() {
  // Visit all instructions and create LIR graph
  HIRInstruction* hinstr = hir()->first_instruction();

  for (; hinstr != NULL; hinstr = hinstr->next()) {
    if (hinstr->ast() != NULL && hinstr->ast()->offset() != -1) {
      heap()->source_map()->Push(masm()->offset(), hinstr->ast()->offset());
    }

    // Each function has separate spill slots.
    // Finalize previous function's spills (if there was any) and
    // prepare entering new function
    if (hinstr->next() == NULL ||
        hinstr->next()->type() == HIRInstruction::kEntry) {
      // One spill is always present (tmp spill for parallel move)!
      // while (!spills()->IsEmpty()) spills()->Get();
    }
  }

  char out[64 * 1024];
  hir()->Print(out, sizeof(out));
  fprintf(stdout, "---------------------------------\n%s\n", out);

  Print(out, sizeof(out));
  fprintf(stdout, "%s\n", out);
}


void LIR::Generate() {
  LIRInstruction* instr = first_instruction_;

  for (; instr != NULL; instr = instr->next()) {
    // relocate all instruction' uses
    instr->Relocate(masm());

    if (instr->type() != LIRInstruction::kParallelMove) {
      // generate instruction itself
      masm()->spill_offset(instr->spill_offset() * HValue::kPointerSize);
      instr->masm(masm());
      instr->Generate();
    }

    // prepare entering new function
    if (instr->next() == NULL) {
      masm()->FinalizeSpills(instr->spill_offset() - 1);
    } else if (instr->next()->type() == LIRInstruction::kEntry &&
               instr->prev() != NULL) {
      masm()->FinalizeSpills(instr->prev()->spill_offset() - 1);
    }
  }
}


void LIR::Print(char* buffer, uint32_t size) {
  PrintBuffer p(buffer, size);

  LIRInstruction* instr = first_instruction_;
  for (; instr != NULL; instr = instr->next()) {
    instr->Print(&p);
    if (instr->next() != NULL) p.Print("\n");
  }

  p.Finalize();
}

} // namespace internal
} // namespace candor
