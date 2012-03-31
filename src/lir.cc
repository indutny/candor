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

#include <stdlib.h> // NULL

namespace candor {
namespace internal {

LIR::LIR(Heap* heap, HIR* hir) : heap_(heap), hir_(hir) {
  // Calculate numeric liveness ranges
  CalculateLiveness();

  // Put each register to the the list
  for (int i = kLIRRegisterCount - 1; i >= 0; --i) {
    registers()->Release(i);
  }
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


void LIR::ExpireOldValues(HIRInstruction* hinstr) {
  int current = hinstr->id();
  while (active_values()->length() > 0 &&
         current > active_values()->head()->value()->live_range()->start) {
    HIRValue* value = active_values()->Shift();
    if (value->operand() == NULL) continue;

    // Return register/spill slot back to freelist
    if (value->operand()->is_register()) {
      registers()->Release(value->operand()->value());
    } else if (value->operand()->is_spill()) {
      spills()->Release(value->operand()->value());
    }

    // Reset operand
    value->operand(NULL);
  }
}


void LIR::SpillAllocated(HIRParallelMove* move) {
  HIRValue* value;
  do {
    value = spill_candidates()->Shift();
  } while (value != NULL && !value->operand()->is_register());

  assert(value != NULL);

  int spill_index;
  if (spills()->IsEmpty()) {
    spill_index = get_new_spill();
  } else {
    spill_index = spills()->Get();
  }

  LIROperand* spill = new LIROperand(LIROperand::kSpill, spill_index);

  move->AddMove(value->operand(), spill);

  registers()->Release(value->operand()->value());
  value->operand(spill);
}


void LIR::AddToSpillCandidates(HIRValue* value) {
  int end = value->live_range()->end;
  HIRValueList::Item* item = spill_candidates()->head();
  for (; item != NULL; item = item->next()) {
    HIRValue* candidate_value = item->value();

    // Some value in the list is less than current
    // insert before it
    if (candidate_value->live_range()->end < end) {
      HIRValueList::Item* new_candidate = new HIRValueList::Item(value);

      if (item->prev() != NULL) item->prev()->next(new_candidate);
      new_candidate->prev(item->prev());
      new_candidate->next(item);
      item->prev(new_candidate);

      break;
    }
  }

  // End of list was reached - just push value
  if (item == NULL) {
    spill_candidates()->Push(value);
  }
}


void LIR::GenerateInstruction(Masm* masm, HIRInstruction* hinstr) {
  // Parallel move instruction doesn't need allocation
  if (hinstr->type() == HIRInstruction::kParallelMove) return;

  ExpireOldValues(hinstr);
  HIRValueList::Item* item = hinstr->values()->tail();
  HIRParallelMove* move = NULL;

  for (; item != NULL; item = item->prev()) {
    HIRValue* value = item->value();

    // Skip immediate values
    if (value->slot()->is_immediate()) {
      value->operand(new LIROperand(LIROperand::kImmediate, value->slot()->value()));
      continue;
    }

    // Skip already allocated values
    if (value->operand() != NULL) continue;

    // Get register (spill if all registers are in use)
    if (registers()->IsEmpty()) {
      // Lazily create parallel move instruction
      if (move == NULL) {
        // NOTE: hinstr->id() is always even, odds are reserved for moves
        move = new HIRParallelMove();
        move->Init(hinstr->block(), hinstr->id() + 1);

        // Insert `move` into instruction's linked-list
        // (for debug purposes)
        move->next(hinstr);
        if (hinstr->prev() != NULL) hinstr->prev()->next(move);
        hinstr->prev(move);
      }

      // Spill some allocated register and try again
      SpillAllocated(move);
    }
    int reg = registers()->Get();
    value->operand(new LIROperand(LIROperand::kRegister, reg));

    // Amend active values and spill candidates
    active_values()->Push(value);

    AddToSpillCandidates(value);
    {
    }
  }

  // All registers was allocated, perform move if needed
  if (move != NULL) {
    LIRInstruction* lmove = Cast(move);

    lmove->masm(masm);
    lmove->Generate();
  }

  // Generate instruction itself
  LIRInstruction* linstr = Cast(hinstr);

  linstr->masm(masm);
  linstr->Generate();
}


void LIR::Generate(Masm* masm) {
  // Visit all instructions
  HIRInstruction* hinstr = hir()->first_instruction();

  for (; hinstr != NULL; hinstr = hinstr->next()) {
    GenerateInstruction(masm, hinstr);
  }
}

} // namespace internal
} // namespace candor
