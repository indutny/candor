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

#include <stdlib.h> // NULL

namespace candor {
namespace internal {

LIR::LIR(Heap* heap, HIR* hir) : heap_(heap), hir_(hir), spill_count_(0) {
  // Calculate numeric liveness ranges
  CalculateLiveness();

  // Prune phis that ain't used
  PrunePhis();

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
    int min = -1;
    int max = -1;
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


void LIR::PrunePhis() {
  HIRPhiList::Item* item = hir()->phis()->head();
  for (; item != NULL; item = item->next()) {
    HIRPhi* value = HIRPhi::Cast(item->value());
    HIRValue::LiveRange* phi = value->live_range();

    // Skip dead phis
    if (phi->start == -1) continue;

    // Add phi to predecessor's gotos
    for (int i = 0; i < value->block()->predecessors_count(); i++) {
      value->block()->predecessors()[0]->
          last_instruction()->values()->Push(value);
    }

    // We'll extend liveness of phi's inputs
    // to ensure that we can always do movement at the end of blocks that
    // contains those inputs
    HIRValueList::Item* input_item = value->inputs()->head();
    for (; input_item != NULL; input_item = input_item->next()) {
      HIRValue* input = input_item->value();
      HIRValue::LiveRange* range = input->live_range();

      // Skip dead inputs
      if (range->start == -1) continue;

      for (int i = 0; i < 2; i++) {
        HIRBasicBlock* block = value->block()->predecessors()[i];
        if (!input->block()->Dominates(block)) continue;

        HIRInstruction* last = block->last_instruction();

        if (range->start > last->id()) range->start = last->id();
        if (range->end < last->id()) range->end = last->id();

        // And push it to the goto
        last->values()->Push(input_item->value());
      }
    }
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


int LIR::AllocateRegister(HIRInstruction* hinstr) {
  // Get register (spill if all registers are in use).
  if (registers()->IsEmpty()) {
    // Spill some allocated register on failure and try again
    HIRValue* value;
    do {
      value = spill_candidates()->Shift();
    } while (value != NULL && !value->operand()->is_register());

    assert(value != NULL);

    LIROperand* spill = GetSpill();
    HIRParallelMove::GetBefore(hinstr)->AddMove(value->operand(), spill);

    registers()->Release(value->operand()->value());
    value->operand(spill);
  }

  return registers()->Get();
}


void LIR::SpillActive(Masm* masm,
                      HIRInstruction* hinstr,
                      LIROperandList* release_list) {
  HIRValueList::Item* item = active_values()->head();
  HIRParallelMove* move = NULL;
  HIRParallelMove* reverse_move = NULL;

  for (; item != NULL; item = item->next()) {
    HIRValue* value = item->value();

    // Do not spill instruction's result
    if (value == hinstr->GetResult()) continue;

    // Do not spill values that are dead after instruction with side-effect
    // or are just not registers
    if (value->live_range()->end <= hinstr->id() ||
        !value->operand()->is_register()) {
      continue;
    }

    // Lazily allocate move instructions
    move = HIRParallelMove::GetBefore(hinstr);
    reverse_move = HIRParallelMove::GetAfter(hinstr);

    LIROperand* spill = GetSpill();
    move->AddMove(value->operand(), spill);
    reverse_move->AddMove(spill, value->operand());

    // Spill should be released after instruction
    release_list->Push(spill);
  }
}


void LIR::MovePhis(HIRInstruction* hinstr) {
  HIRBasicBlock* succ = hinstr->block()->successors()[0];
  HIRPhiList::Item* item = succ->phis()->head();

  // Iterate through phis
  for (; item != NULL; item = item->next()) {
    HIRPhi* phi = item->value();

    HIRValueList::Item* value = phi->inputs()->head();
    for (; value != NULL; value = value->next()) {
      // Skip non-local and dead inputs
      if (!value->value()->block()->Dominates(hinstr->block()) ||
          value->value()->operand() == NULL ||
          phi->operand() == NULL) {
        continue;
      }

      // Add movement from hinstr block's input to phi
      HIRParallelMove::GetBefore(hinstr)->
          AddMove(value->value()->operand(), phi->operand());

      // Only one move per phi
      break;
    }
  }
}


void LIR::GenerateInstruction(Masm* masm, HIRInstruction* hinstr) {
  LIRInstruction* linstr = Cast(hinstr);

  // Relocate all block's uses
  // NOTE: ParallelMove doesn't have block associated with them
  if (hinstr->block() != NULL && hinstr->block()->uses()->length() > 0) {
    RelocationInfo* block_reloc;
    while ((block_reloc = hinstr->block()->uses()->Shift()) != NULL) {
      block_reloc->target(masm->offset());
      masm->relocation_info_.Push(block_reloc);
    }
  }

  // Parallel move instruction doesn't need allocation nor generation,
  // because it should be generated automatically.
  if (hinstr->type() == HIRInstruction::kParallelMove) {
    // However if it's last instruction - it won't be generated automatically
    if (hinstr->next() == NULL) {
      HIRParallelMove::Cast(hinstr)->Reorder();

      linstr->masm(masm);
      linstr->Generate();
    }
    return;
  }

  // List of operand that should be `released` after instruction
  LIROperandList release_list;

  ExpireOldValues(hinstr);

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

    int reg = AllocateRegister(hinstr);
    value->operand(new LIROperand(LIROperand::kRegister, reg));

    // Amend active values and spill candidates
    active_values()->Push(value);

    AddToSpillCandidates(value);
  }

  // Allocate scratch registers
  int i;
  for (i = 0; i < linstr->scratch_count(); i++) {
    int reg = AllocateRegister(hinstr);
    linstr->scratches[i] = new LIROperand(LIROperand::kRegister, reg);
    release_list.Push(linstr->scratches[i]);
  }

  // Allocate result
  if (hinstr->GetResult() != NULL) {
    HIRValue* value = hinstr->GetResult();
    if (value->operand() == NULL) {
      int reg = AllocateRegister(hinstr);
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
    if (linstr->inputs[i] == NULL) {
      // Instruction hasn't specified inputs restrictions
      linstr->inputs[i] = item->value()->operand();
    } else if (!linstr->inputs[i]->is_equal(item->value()->operand())) {
      assert(!linstr->inputs[i]->is_immediate());

      // Instruction specified inputs restrictions,
      // create movement if needed
      LIROperand* spill;

      HIRParallelMove* move = HIRParallelMove::GetBefore(hinstr);
      HIRParallelMove* reverse_move = HIRParallelMove::GetAfter(hinstr);

      move->AddMove(item->value()->operand(), linstr->inputs[i]);
      reverse_move->AddMove(linstr->inputs[i], item->value()->operand());

      // Spill input operand if it's in use
      if (IsInUse(linstr->inputs[i])) {
        // Value may be spilled either in register or a spill
        if (registers()->IsEmpty() || hinstr->HasSideEffects()) {
          spill = GetSpill();
        } else {
          spill = new LIROperand(LIROperand::kRegister, registers()->Get());
        }

        move->AddMove(linstr->inputs[i], spill);
        reverse_move->AddMove(spill, linstr->inputs[i]);

        // Spill should be released after instruction
        release_list.Push(spill);
      }
    }

    i++;
  }

  // All active registers should be spilled before entering
  // instruction with side effects (like stubs)
  //
  // Though instruction itself should receive arguments in registers
  // (if that's possible)
  if (hinstr->HasSideEffects()) SpillActive(masm, hinstr, &release_list);

  // If instruction is a kGoto to the join block,
  // add join's phis to the movement
  if (hinstr->is(HIRInstruction::kGoto) &&
      hinstr->block()->successors_count() == 1 &&
      hinstr->block()->successors()[0]->predecessors_count() == 2) {
    MovePhis(hinstr);
  }

  // All registers was allocated, perform move if needed
  {
    HIRParallelMove* move = HIRParallelMove::GetBefore(hinstr);
    LIRInstruction* lmove = Cast(move);

    // Order movements (see Parallel Move paper)
    move->Reorder();

    lmove->masm(masm);
    lmove->Generate();
  }

  // Generate instruction itself
  linstr->masm(masm);
  linstr->Generate();

  LIROperand* release_op;
  while ((release_op = release_list.Shift()) != NULL) {
    if (release_op->is_register()) {
      registers()->Release(release_op->value());
    } else if (release_op->is_spill()) {
      spills()->Release(release_op->value());
    }
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
