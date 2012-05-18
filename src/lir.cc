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

LIRSpillList::~LIRSpillList() {
  HIRValue* value;
  while ((value = Shift()) != NULL) {
    lir()->spill_values()->InsertSorted<HIRValueEndShape>(value);
  }
}


LIR::LIR(Heap* heap, HIR* hir, Masm* masm) : heap_(heap),
                                             hir_(hir),
                                             spill_count_(0),
                                             tmp_spill_(GetSpill()),
                                             first_instruction_(NULL),
                                             last_instruction_(NULL) {
  // Calculate numeric liveness ranges
  CalculateLiveness();

  // Prune phis that ain't used
  PrunePhis();

  // Put each register to the the list
  for (int i = kLIRRegisterCount - 1; i >= 0; --i) {
    registers()->Release(i);
  }

  Translate(masm);
  Generate(masm);
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

    // Extend phi's and it's inputs liveness
    if (value->is_phi() && max != -1) {
      ZoneList<HIRPhi*> work_list;
      work_list.Push(HIRPhi::Cast(value));

      HIRPhi* phi;
      while ((phi = work_list.Shift()) != NULL) {
        int id = phi->block()->first_instruction()->id();
        if (phi->live_range()->Extend(id)) {
          if (phi->inputs()->head()->value()->is_phi()) {
            work_list.Push(HIRPhi::Cast(phi->inputs()->head()->value()));
          }
          if (phi->inputs()->tail()->value()->is_phi()) {
            work_list.Push(HIRPhi::Cast(phi->inputs()->tail()->value()));
          }
        }
      }
    }
  }

  // Add non-local variable uses to the end of loop
  // to ensure that they will be live after the first loop's pass
  HIRInstruction* hinstr = hir()->first_instruction();
  for (; hinstr != NULL; hinstr = hinstr->next()) {
    if (hinstr->block() == NULL || !hinstr->block()->is_loop_start() ||
        hinstr != hinstr->block()->first_instruction()) {
      continue;
    }
    HIRBasicBlock* end = HIRLoopStart::Cast(hinstr->block())->end();

    item = hinstr->block()->values()->head();
    for (; item != NULL; item = item->next()) {
      HIRValue* value = item->value();

      if (value == NULL || value->live_range()->start == -1) continue;

      value->live_range()->Extend(end->last_instruction()->id());
    }
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
      if (i == 0) {
        // left input (closer)
        input = value->inputs()->head()->value();
      } else {
        // Right input
        input = value->inputs()->tail()->value();
      }

      HIRValue::LiveRange* range = input->live_range();

      // Skip dead inputs
      if (range->start == -1) continue;

      // Input should be available in last block's instruction
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


LIROperand* LIR::GetRegister(HIRInstruction* hinstr) {
  // Get register (spill if all registers are in use).
  if (registers()->IsEmpty()) {
    // Spill some allocated register on failure and try again
    HIRValue* spill_cand = spill_values()->Pop();
    assert(spill_cand != NULL);
    assert(spill_cand->operand()->is_register());

    GetSpill(hinstr, spill_cand);
  }

  return new LIROperand(LIROperand::kRegister, registers()->Get());
}


LIROperand* LIR::GetRegister(HIRInstruction* hinstr, HIRValue* value) {
  spill_list()->Push(value);
  active_values()->InsertSorted<HIRValueEndShape>(value);

  if (value->operand() != NULL && value->operand()->is_register()) {
    return value->operand();
  }

  // Get new register and put it into value
  LIROperand* reg = GetRegister(hinstr);
  ChangeOperand(hinstr, value, reg);

  return reg;
}


LIROperand* LIR::GetRegister(HIRInstruction* hinstr,
                             HIRValue* value,
                             LIROperand* reg) {
  spill_list()->Push(value);
  active_values()->InsertSorted<HIRValueEndShape>(value);

  if (value->operand() != NULL && value->operand()->is_equal(reg)) {
    return value->operand();
  }

  HIRValueList::Item* item = active_values()->tail();
  for (; item != NULL; item = item->prev()) {
    if (item->value()->operand()->is_equal(reg)) {
      GetSpill(hinstr, item->value());
    }
  }

  registers()->Remove(reg->value());
  ChangeOperand(hinstr, value, reg);
}


LIROperand* LIR::GetSpill() {
  int spill_index;
  if (spills()->IsEmpty()) {
    spill_index = get_new_spill();
  } else {
    spill_index = spills()->Get();
  }

  return new LIROperand(LIROperand::kSpill, spill_index);
}


LIROperand* LIR::GetSpill(HIRInstruction* hinstr, HIRValue* value) {
  active_values()->InsertSorted<HIRValueEndShape>(value);

  if (value->operand() != NULL && value->operand()->is_spill()) {
    return value->operand();
  }

  LIROperand* spill = GetSpill();
  ChangeOperand(hinstr, value, spill);

  return spill;
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

    LIRInstruction* linstr = hinstr->lir(this);
    LIROperandList::Item* op = linstr->operands()->head();
    for (; op != NULL; op = op->next()) {
      if (op->value()->hir() == value) linstr->operands()->Remove(op);
    }

    LIROperand* from = value->operand();
    LIROperand* to = GetSpill(hinstr, value);

    if (linstr->prev() != NULL) {
      op = linstr->prev()->operands()->head();
      while (op != NULL && op->value()->hir() != value) {
        op = op->next();
      }

      if (op != NULL && !op->value()->is_equal(from)) {
        HIRParallelMove::GetBefore(hinstr)->AddMove(
            op->value(),
            from);
      }
    }
    hinstr->lir(this)->operands()->Push(GetSpill(hinstr, value));
  }
}


void LIR::TranslateInstruction(Masm* masm, HIRInstruction* hinstr) {
  LIRInstruction* linstr = hinstr->lir(this);

  // List of operand that should be `released` after instruction
  LIRSpillList sl(this);
  spill_list(&sl);

  if (hinstr->type() != HIRInstruction::kParallelMove) {
    HIRParallelMove* move = HIRParallelMove::GetBefore(hinstr);
    AddInstruction(move->lir(this));
  }

  ExpireOldValues(hinstr, active_values());
  ExpireOldValues(hinstr, spill_values());

  // Parallel move instruction doesn't need allocation nor generation,
  // because it should be generated automatically.
  if (hinstr->type() == HIRInstruction::kParallelMove) {
    return;
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
  for (i = 0; item != NULL; item = item->next(), i++) {
    HIRValue* value = item->value();

    // Allocate immediate values
    if (value->slot()->is_immediate() && value->operand() == NULL) {
      value->operand(new LIROperand(LIROperand::kImmediate,
                                    value->slot()->value()));
    }

    if (i < linstr->input_count()) {
      if (linstr->inputs[i] != NULL) {
        // Instruction requires it's input to reside in some specific register
        GetRegister(hinstr, value, linstr->inputs[i]);
      } else if (value->operand() != NULL && !value->operand()->is_spill()) {
        linstr->inputs[i] = value->operand();
      } else {
        LIROperand* op = GetRegister(hinstr, value);
        linstr->inputs[i] = op;
      }
    } else {
      if (value->operand() != NULL) continue;
      GetRegister(hinstr, value);
    }
  }

  // Allocate scratch registers
  for (i = 0; i < linstr->scratch_count(); i++) {
    linstr->scratches[i] = GetRegister(hinstr);

    // Cleanup scratch registers after usage
    HIRParallelMove::GetAfter(hinstr)->AddMove(
        new LIROperand(LIROperand::kImmediate, static_cast<off_t>(0)),
        linstr->scratches[i]);
  }

  // Allocate result
  if (hinstr->GetResult() != NULL) {
    HIRValue* value = hinstr->GetResult();
    if (value->slot()->is_immediate()) {
      value->operand(new LIROperand(LIROperand::kImmediate,
                                    value->slot()->value()));
    } else {
      GetRegister(hinstr, value);
    }
    linstr->result = value->operand();
  }

  HIRParallelMove::GetBefore(hinstr)->Reorder(this);

  AddInstruction(linstr);

  // Store LIR arguments
  {
    HIRValueList::Item* item = NULL;

    if (hinstr->type() == HIRInstruction::kEntry) {
      item = reinterpret_cast<HIREntry*>(hinstr)->args()->head();
    } else if (hinstr->type() == HIRInstruction::kCall) {
      item = reinterpret_cast<HIRCall*>(hinstr)->args()->head();
    }

    for (; item != NULL; item = item->next()) {
      linstr->args()->Push(item->value()->operand());
    }
  }

  // All active registers should be spilled before entering
  // instruction with side effects (like stubs)
  //
  // Though instruction itself should receive arguments in registers
  // (if that's possible)
  if (hinstr->HasSideEffects()) SpillActive(masm, hinstr);

  linstr->spill_offset(spill_count() + 1);

  // Release scratches
  for (i = 0; i < linstr->scratch_count(); i++) {
    Release(linstr->scratches[i]);
  }

  // Finalize next movement instruction
  HIRParallelMove::GetAfter(hinstr)->Reorder(this);
}


void LIR::Translate(Masm* masm) {
  // Visit all instructions and create LIR graph
  HIRInstruction* hinstr = hir()->first_instruction();

  for (; hinstr != NULL; hinstr = hinstr->next()) {
    if (hinstr->ast() != NULL && hinstr->ast()->offset() != -1) {
      heap()->source_map()->Push(masm->offset(), hinstr->ast()->offset());
    }

    TranslateInstruction(masm, hinstr);

    if (hinstr->block() != NULL &&
        hinstr == hinstr->block()->first_instruction()) {
      HIRPhiList::Item* phi_item = hinstr->block()->phis()->head();
      for (; phi_item != NULL; phi_item = phi_item->next()) {
        HIRPhi* phi = phi_item->value();

        HIRValueList::Item* val_item = phi->inputs()->head();
        for (; val_item != NULL; val_item = val_item->next()) {
          LIROperand* op = new LIROperand(*phi->operand());
          op->hir(val_item->value());

          hinstr->lir(this)->operands()->Push(op);
        }
      }
    }

    // Each function has separate spill slots.
    // Finalize previous function's spills (if there was any) and
    // prepare entering new function
    if (hinstr->next() == NULL ||
        hinstr->next()->type() == HIRInstruction::kEntry) {
      // One spill is always present (tmp spill for parallel move)!
      spill_count(1);
      while (!spills()->IsEmpty()) spills()->Get();
    }
  }

  char out[5000];
  hir()->Print(out, sizeof(out));
  fprintf(stdout, "---------------------------------\n%s\n", out);
}


void LIR::GenerateShuffle(Masm* masm,
                          LIRInstruction* from,
                          LIRInstruction* to) {
  HIRParallelMove* move = new HIRParallelMove();
  move->Init(NULL);

  LIROperandList::Item* i;
  for (i = to->operands()->head(); i != NULL; i = i->next()) {
    LIROperand* to_op = i->value();

    if (to_op->hir()->slot()->is_immediate()) {
      move->AddMove(new LIROperand(LIROperand::kImmediate,
                                   to_op->hir()->slot()->value()),
                    to_op);
      continue;
    }

    LIROperandList::Item* j;
    for (j = from->operands()->head(); j != NULL; j = j->next()) {
      LIROperand* from_op = j->value();

      if (from_op->hir() != to_op->hir() || from_op->is_equal(to_op)) continue;

      move->AddMove(from_op, to_op);
    }
  }

  LIRInstruction* lmove = move->lir(this);
  lmove->masm(masm);
  lmove->Generate();
}


void LIR::Generate(Masm* masm) {
  LIRInstruction* instr = first_instruction_;

  for (; instr != NULL; instr = instr->next()) {
    // relocate all instruction' uses
    instr->Relocate(masm);

    // generate instruction itself
    masm->spill_offset(instr->spill_offset() * HValue::kPointerSize);
    instr->masm(masm);
    instr->Generate();

    if (instr->next() != NULL) GenerateShuffle(masm, instr, instr->next());

    // prepare entering new function
    if ((instr->next() == NULL ||
        instr->next()->type() == LIRInstruction::kEntry) &&
        instr->prev() != NULL) {
      masm->FinalizeSpills(instr->prev()->spill_offset() - 1);
    }
  }
}

} // namespace internal
} // namespace candor
