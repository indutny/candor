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

LIRReleaseList::~LIRReleaseList() {
  LIROperand* release_op;
  while ((release_op = Shift()) != NULL) lir()->Release(release_op);
}


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
      value->block()->predecessors()[i]->
          last_instruction()->values()->Push(value);
    }

    // We'll extend liveness of phi's inputs
    // to ensure that we can always do movement at the end of blocks that
    // contains those inputs
    for (int i = 0; i < 2; i++) {
      HIRBasicBlock* block = value->block()->predecessors()[i];
      HIRInstruction* last = block->last_instruction();

      HIRValue* input;
      if (last->next()->next() == value->block()->first_instruction()) {
        // left input (closer)
        input = value->inputs()->head()->value();
      } else {
        // Right input
        input = value->inputs()->tail()->value();
      }

      HIRValue::LiveRange* range = input->live_range();

      // Skip dead inputs
      if (range->start == -1) continue;

      range->Extend(last->id());
      phi->Extend(last->id());

      // And push it to the goto
      last->values()->Push(input);
    }
  }
}


void LIR::ExpireOldValues(HIRInstruction* hinstr, ZoneList<HIRValue*>* list) {
  int current = hinstr->id();
  while (list->length() > 0 &&
         current > list->head()->value()->live_range()->end) {
    HIRValue* value = list->Shift();
    if (value->operand() == NULL) continue;

    // Return register/spill slot back to freelist
    Release(value->operand());

    // Reset operand
    value->operand(NULL);
  }
}


LIROperand* LIR::AllocateRegister(HIRInstruction* hinstr) {
  // Get register (spill if all registers are in use).
  if (registers()->IsEmpty()) {
    // Spill some allocated register on failure and try again
    HIRValue* value;
    do {
      value = spill_values()->Pop();
    } while (value != NULL && !value->operand()->is_register() &&
             value->live_range()->end < hinstr->id());

    assert(value != NULL);

    LIROperand* spill = GetSpill();
    HIRParallelMove::GetBefore(hinstr)->AddMove(value->operand(), spill);

    registers()->Release(value->operand()->value());
    value->operand(spill);
  }

  return new LIROperand(LIROperand::kRegister, registers()->Get());
}


void LIR::SpillRegister(HIRInstruction* hinstr, LIROperand* reg) {
  HIRValueList::Item* item = active_values()->head();
  LIROperand* spill = NULL;
  for (; item != NULL; item = item->next()) {
    HIRValue* value = item->value();
    if (value->operand() == NULL || !value->operand()->is_equal(reg)) {
      continue;
    }

    // Lazily allocate spill
    if (spill == NULL) {
      if (registers()->IsEmpty()) {
        spill = GetSpill();
      } else {
        spill = AllocateRegister(hinstr);
      }
    }

    // Move value to spill/register
    value->operand(spill);
  }

  // If at least one value was moved - insert movement
  if (spill != NULL) {
    HIRParallelMove::GetBefore(hinstr)->AddMove(reg, spill);

    // Commit movement as swapping registers is not a parallel
    // move and may introduce conflicts:
    // eax <-> ebx
    // ebx <-> ecx
    HIRParallelMove::GetBefore(hinstr)->Reorder(this);
  }
}


void LIR::InsertMoveSpill(HIRParallelMove* move,
                          HIRParallelMove* reverse,
                          LIROperand* spill) {
  HIRValue* value = new HIRValue(reverse->block());
  value->live_range()->start = move->id();
  value->live_range()->end = reverse->id();
  value->operand(spill);
  active_values()->InsertSorted<HIRValueEndShape>(value);
}


void LIR::SpillActive(Masm* masm, HIRInstruction* hinstr) {
  HIRValueList::Item* item = active_values()->head();
  HIRParallelMove* move = HIRParallelMove::GetBefore(hinstr);
  HIRParallelMove* reverse_move = HIRParallelMove::GetAfter(hinstr);

  // Create map of movement
  LIROperand* spills[kLIRRegisterCount];
  for (int i = 0; i < kLIRRegisterCount; i++) {
    spills[i] = NULL;
  }

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

    // Skip already spilled registers
    int reg = value->operand()->value();
    if (spills[reg] != NULL) continue;

    spills[reg] = GetSpill();
    move->AddMove(value->operand(), spills[reg]);
    reverse_move->AddMove(spills[reg], value->operand());

    // Spill should be released after instruction
    InsertMoveSpill(move, reverse_move, spills[reg]);
  }
}


void LIR::MovePhis(HIRInstruction* hinstr) {
  HIRBasicBlock* succ = hinstr->block()->successors()[0];
  HIRPhiList::Item* item = succ->phis()->head();

  // Iterate through phis
  for (; item != NULL; item = item->next()) {
    HIRPhi* phi = item->value();
    HIRValue* input;

    if (hinstr->next()->next() == succ->first_instruction()) {
      // left input (closer)
      input = phi->inputs()->head()->value();
    } else {
      // Right input
      input = phi->inputs()->tail()->value();
    }

    // Skip dead phis/inputs
    if (input->operand() == NULL || phi->operand() == NULL) {
      continue;
    }

    // Add movement from hinstr block's input to phi
    HIRParallelMove::GetBefore(hinstr)->
        AddMove(input->operand(), phi->operand());
  }
}


void LIR::StoreLoopInvariants(HIRBasicBlock* block,
                              ZoneList<HIRLoopShuffle*>* shuffle) {
  // Store all `live` variables from condition block
  ZoneList<HIRValue*>::Item* item = block->values()->head();
  for (; item != NULL; item = item->next()) {
    if (item->value()->operand() == NULL ||
        item->value()->operand()->is_immediate()) {
      continue;
    }

    shuffle->Push(new HIRLoopShuffle(item->value(),
                                     item->value()->operand()));
  }
}


void LIR::ApplyShuffle(HIRInstruction* hinstr, ShuffleDirection direction) {
  ZoneList<HIRLoopShuffle*>* list;
  HIRLoopShuffle* shuffle;
  HIRParallelMove* move;

  if (direction == kShuffleBefore) {
    list = hinstr->block()->loop_preshuffle();
    move = HIRParallelMove::GetBefore(hinstr);
  } else {
    list = hinstr->block()->loop_postshuffle();
    move = HIRParallelMove::GetAfter(hinstr);
  }

  if (list->length() == 0) return;

  if (direction == kShuffleBefore) move->Reorder(this);

  while ((shuffle = list->Shift()) != NULL) {
    // Skip dead values or values with unchanged operand
    if (shuffle->value()->operand() == NULL ||
        shuffle->operand()->is_equal(shuffle->value()->operand())) {
      continue;
    }

    if (direction == kShuffleBefore) {
      move->AddMove(shuffle->value()->operand(), shuffle->operand());
    } else {
      // After shuffle should set only live variables
      if (shuffle->value()->live_range()->end >= hinstr->id() + 1) {
        move->AddMove(shuffle->operand(), shuffle->value()->operand());
      }
    }
  }

  if (direction == kShuffleAfter) move->Reorder(this);
}


void LIR::GenerateReverseMove(Masm* masm, HIRInstruction* hinstr) {
  if (hinstr->next()->type() != HIRInstruction::kParallelMove) return;

  HIRParallelMove* move = HIRParallelMove::Cast(hinstr->next());
  LIRInstruction* lmove = Cast(move);
  move->Reorder(this);

  lmove->masm(masm);
  lmove->Generate();

  // Reset all movements
  move->Reset();
}


void LIR::GenerateInstruction(Masm* masm, HIRInstruction* hinstr) {
  LIRInstruction* linstr = Cast(hinstr);

  // Relocate all block's uses
  if (hinstr->block() != NULL) hinstr->block()->Relocate(masm);

  // List of operand that should be `released` after instruction
  LIRReleaseList release_list(this);

  // Parallel move instruction doesn't need allocation nor generation,
  // because it should be generated automatically.
  if (hinstr->type() == HIRInstruction::kParallelMove) {
    // However if it's last instruction - it won't be generated automatically
    if (hinstr->next() == NULL) {
      HIRParallelMove::Cast(hinstr)->Reorder(this);

      linstr->masm(masm);
      linstr->Generate();
    }
    return;
  }

  ExpireOldValues(hinstr, active_values());
  ExpireOldValues(hinstr, spill_values());

  // If we're at the loop start - record preshuffle values
  if (hinstr->block() != NULL && hinstr->block()->is_loop_start() &&
      hinstr == hinstr->block()->first_instruction()) {
    HIRLoopStart* loop_start = HIRLoopStart::Cast(hinstr->block());
    HIRBasicBlock* loop_body = loop_start->body();
    StoreLoopInvariants(loop_start, loop_body->loop_preshuffle());
  }

  // If instruction has input restrictions - ensure that those registers can't
  // be allocated for spills or anything
  int i;
  for (i = 0; i < linstr->input_count(); i++) {
    if (linstr->inputs[i] == NULL) continue;
    assert(linstr->inputs[i]->is_register());

    registers()->Remove(linstr->inputs[i]->value());
  }

  // Allocate all values used in instruction
  HIRValueList::Item* item = hinstr->values()->head();
  for (i = -1; item != NULL; item = item->next()) {
    HIRValue* value = item->value();

    // Increment input index
    if (value != hinstr->GetResult()) i++;

    // Allocate immediate values
    if (value->slot()->is_immediate()) {
      value->operand(new LIROperand(LIROperand::kImmediate,
                                    value->slot()->value()));
      if (linstr->inputs[i] == NULL) continue;
    }

    // Result will be allocated later
    if (value == hinstr->GetResult()) continue;
    if (i < linstr->input_count() && linstr->inputs[i] != NULL) {
      // Instruction requires it's input to reside in some specific register

      if (value->operand() != NULL) {
        // If input contains required register operand - skip this value
        if (value->operand()->is_equal(linstr->inputs[i])) continue;

        // Move value to register
        HIRParallelMove::GetBefore(hinstr)->AddMove(value->operand(),
                                                    linstr->inputs[i]);
        Release(value->operand());
      }

      // Move all uses of this register into spill/other register
      SpillRegister(hinstr, linstr->inputs[i]);

      value->operand(linstr->inputs[i]);
    } else {
      // Skip already allocated values
      if (value->operand() != NULL) continue;

      value->operand(AllocateRegister(hinstr));
      spill_values()->InsertSorted<HIRValueEndShape>(value);
    }

    // Amend active values
    active_values()->InsertSorted<HIRValueEndShape>(value);
  }

  // Allocate scratch registers
  for (i = 0; i < linstr->scratch_count(); i++) {
    linstr->scratches[i] = AllocateRegister(hinstr);
    release_list.Push(linstr->scratches[i]);

    // Cleanup scratch registers after usage
    HIRParallelMove::GetAfter(hinstr)->AddMove(
        new LIROperand(LIROperand::kImmediate, static_cast<off_t>(0)),
        linstr->scratches[i]);
  }

  // Allocate result
  if (hinstr->GetResult() != NULL) {
    HIRValue* value = hinstr->GetResult();
    if (value->operand() == NULL) {
      value->operand(AllocateRegister(hinstr));
      spill_values()->InsertSorted<HIRValueEndShape>(value);
      active_values()->InsertSorted<HIRValueEndShape>(value);
    }
    linstr->result = value->operand();
  }

  // Set inputs
  item = hinstr->values()->head();
  for (i = 0; i < linstr->input_count(); item = item->next()) {
    if (item->value() == hinstr->GetResult()) continue;

    assert(item->value()->operand() != NULL);
    if (linstr->inputs[i] == NULL) {
      linstr->inputs[i] = item->value()->operand();
    }

    i++;
  }

  // If we're at loop's branch instruction - record postshuffle
  if (hinstr->block() != NULL && hinstr->block()->is_loop_start() &&
      hinstr == hinstr->block()->last_instruction()) {
    HIRLoopStart* loop_start = HIRLoopStart::Cast(hinstr->block());
    HIRBasicBlock* loop_body = loop_start->body();
    StoreLoopInvariants(loop_start, loop_body->loop_postshuffle());
  }

  // All active registers should be spilled before entering
  // instruction with side effects (like stubs)
  //
  // Though instruction itself should receive arguments in registers
  // (if that's possible)
  if (hinstr->HasSideEffects()) SpillActive(masm, hinstr);

  // If instruction is a kGoto to the join block,
  // add join's phis to the movement
  if (hinstr->is(HIRInstruction::kGoto) &&
      hinstr->block()->successors_count() == 1 &&
      hinstr->block()->successors()[0]->predecessors_count() == 2) {
    MovePhis(hinstr);

    // Apply shuffle to preserve loop invariants
    ApplyShuffle(hinstr, kShuffleBefore);
    ApplyShuffle(hinstr, kShuffleAfter);
  }

  // All registers was allocated, perform move if needed
  {
    HIRParallelMove* move = HIRParallelMove::GetBefore(hinstr);
    LIRInstruction* lmove = Cast(move);

    // Order movements (see Parallel Move paper)
    move->Reorder(this);

    lmove->masm(masm);
    lmove->Generate();
  }

  // Generate instruction itself
  linstr->masm(masm);
  masm->spill_offset((spill_count() + 1) * HValue::kPointerSize);
  linstr->Generate();

  // Finalize next movement instruction
  HIRParallelMove::GetAfter(hinstr)->Reorder(this);
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

  char out[5000];
  hir()->Print(out, sizeof(out));
  fprintf(stdout, "%s\n", out);
}

} // namespace internal
} // namespace candor
