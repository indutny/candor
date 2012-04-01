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

LIR::LIR(Heap* heap, HIR* hir) : heap_(heap), hir_(hir), spill_count_(0) {
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
         current > active_values()->head()->value()->live_range()->end) {
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

  LIROperand* spill = GetSpill();
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


int LIR::AllocateRegister(HIRInstruction* hinstr, HIRParallelMove* &move) {
  // Get register (spill if all registers are in use)
  if (registers()->IsEmpty()) {
    // Lazily create parallel move instruction
    if (move == NULL) {
      // NOTE: hinstr->id() is always even, odds are reserved for moves
      move = new HIRParallelMove(hinstr, HIRParallelMove::kBefore);
    }

    // Spill some allocated register and try again
    SpillAllocated(move);
  }

  return registers()->Get();
}


void LIR::SpillActive(Masm* masm,
                      HIRInstruction* hinstr,
                      HIRParallelMove* &move) {
  HIRValueList::Item* item = active_values()->head();
  HIRParallelMove* reverse_move = NULL;

  for (; item != NULL; item = item->next()) {
    HIRValue* value = item->value();

    // Do not spill values that are dead after instruction with side-effect
    // or are just not registers
    if (value->live_range()->end < hinstr->id() ||
        !value->operand()->is_register()) {
      continue;
    }

    // Lazily allocate move instructions
    if (move == NULL) {
      move = new HIRParallelMove(hinstr, HIRParallelMove::kBefore);
    }
    if (reverse_move == NULL) {
      reverse_move = new HIRParallelMove(hinstr, HIRParallelMove::kAfter);
    }

    LIROperand* spill = GetSpill();
    move->AddMove(value->operand(), spill);
    reverse_move->AddMove(spill, value->operand());
  }

  if (reverse_move != NULL) {
    // Restore spills, they are not used after instr with side-effect
    ZoneList<LIROperand*>::Item* op = reverse_move->raw_sources()->head();
    while (op != NULL) {
      spills()->Release(op->value()->value());
      op = op->next();
    }
  }
}


void LIR::GenerateInstruction(Masm* masm, HIRInstruction* hinstr) {
  LIRInstruction* linstr = Cast(hinstr);

  // Relocate all block's uses
  if (hinstr->block()->uses()->length() > 0) {
    RelocationInfo* block_reloc;
    while ((block_reloc = hinstr->block()->uses()->Shift()) != NULL) {
      block_reloc->target(masm->offset());
      masm->relocation_info_.Push(block_reloc);
    }
  }

  // Parallel move instruction doesn't need allocation nor generation.
  if (hinstr->type() == HIRInstruction::kParallelMove) {
    // However if it's last instruction - it won't be generated automatically
    if (hinstr->next() == NULL) {
      HIRParallelMove::Cast(hinstr)->Reorder();

      linstr->masm(masm);
      linstr->Generate();
    }
    return;
  }

  ExpireOldValues(hinstr);

  // Get previous move instruction (if there're any)
  HIRParallelMove* move = NULL;
  if (hinstr->prev() != NULL &&
      hinstr->prev()->is(HIRInstruction::kParallelMove)) {
    move = HIRParallelMove::Cast(hinstr->prev());
  }

  // Allocate all values used in instruction
  HIRValueList::Item* item = hinstr->values()->head();
  for (; item != NULL; item = item->next()) {
    HIRValue* value = item->value();

    // Allocate immediate values
    if (value->slot()->is_immediate()) {
      value->operand(new LIROperand(LIROperand::kImmediate,
                                    value->slot()->value()));
      continue;
    }

    // Skip result if it's not immediate
    if (value == hinstr->GetResult()) continue;

    // Skip already allocated values
    if (value->operand() != NULL) continue;

    int reg = AllocateRegister(hinstr, move);
    value->operand(new LIROperand(LIROperand::kRegister, reg));

    // Amend active values and spill candidates
    active_values()->Push(value);

    AddToSpillCandidates(value);
  }

  // Allocate scratch registers
  int i;
  for (i = 0; i < linstr->scratch_count(); i++) {
    int reg = AllocateRegister(hinstr, move);
    linstr->scratches[i] = new LIROperand(LIROperand::kRegister, reg);
  }

  // Allocate result
  if (hinstr->GetResult() != NULL) {
    HIRValue* value = hinstr->GetResult();
    if (value->operand() == NULL) {
      int reg = AllocateRegister(hinstr, move);
      value->operand(new LIROperand(LIROperand::kRegister, reg));
      active_values()->Push(value);
      AddToSpillCandidates(value);
    }
    linstr->result = value->operand();
  }

  // Set inputs
  item = hinstr->values()->head();
  for (i = 0; i < linstr->input_count(); item = item->next()) {
    if (item->value() == hinstr->GetResult()) continue;

    assert(item->value()->operand() != NULL);
    linstr->inputs[i++] = item->value()->operand();
  }
  assert(item == NULL || item->next() == NULL);

  // All active registers should be spilled before entering
  // instruction with side effects (like stubs)
  //
  // Though instruction itself should receive arguments in registers
  // (if that's possible)
  if (hinstr->HasSideEffects()) SpillActive(masm, hinstr, move);

  // All registers was allocated, perform move if needed
  if (move != NULL) {
    LIRInstruction* lmove = Cast(move);

    // Order movements (see Parallel Move paper)
    move->Reorder();

    lmove->masm(masm);
    lmove->Generate();
  }

  // Generate instruction itself
  linstr->masm(masm);
  linstr->Generate();

  // Release scratch registeres
  for (i = 0; i < linstr->scratch_count(); i++) {
    registers()->Release(linstr->scratches[i]->value());
  }
}


void LIR::Generate(Masm* masm) {
  // Visit all instructions
  HIRInstruction* hinstr = hir()->first_instruction();

  for (; hinstr != NULL; hinstr = hinstr->next()) {
    GenerateInstruction(masm, hinstr);

    // Each function has separate spill slots.
    // Finalize previous function's spills (if there was any) and
    // prepare entering new function
    if (hinstr->next() == NULL ||
        hinstr->next()->type() == HIRInstruction::kEntry) {
      masm->FinalizeSpills(spill_count());
      spill_count(0);
      while (!spills()->IsEmpty()) spills()->Get();
    }
  }
}

} // namespace internal
} // namespace candor
