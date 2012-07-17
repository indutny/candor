#include "lir.h"
#include "lir-inl.h"
#include "lir-allocator.h"
#include "lir-allocator-inl.h"
#include "lir-instructions.h" // LIRInstruction
#include "hir.h"
#include "hir-instructions.h"

#if CANDOR_ARCH_x64
#include "x64/lir-instructions-x64.h"
#elif CANDOR_ARCH_ia32
#include "ia32/lir-instructions-ia32.h"
#endif
#include "macroassembler.h"
#include "macroassembler-inl.h"

#include <assert.h>
#include <limits.h> // INT_MAX

namespace candor {
namespace internal {

LIRInterval* LIRInterval::SplitAt(int pos) {
  assert((pos & 1) == 0);
  assert(pos >= start());

  // Position points to children's ranges
  if (pos >= end()) {
    LIRIntervalList::Item* item = children()->tail();
    for (; item != NULL; item = item->prev()) {
      if (item->value()->start() >= pos) return item->value()->SplitAt(pos);
    }
  }

  // Position is inside current interval ranges
  LIRInterval* child = new LIRInterval(value());

  child->parent(this);

  this->children()->InsertSorted<LIRIntervalShape>(child);

  // Split ranges
  LIRLiveRange* split_range = last_range();
  while (split_range != NULL && split_range->end() >= pos &&
         split_range->prev() != NULL && split_range->prev()->end() < pos) {
    split_range = split_range->prev();
  }

  if (split_range != NULL && split_range->end() >= pos) {
    // Split range if it is in both parent and child interval
    if (split_range->start() < pos) {
      LIRLiveRange* r = new LIRLiveRange(split_range->start(), pos);
      r->next(split_range);
      r->prev(split_range->prev());

      if (split_range->prev() != NULL) split_range->prev()->next(r);
      split_range->prev(r);

      split_range->start(pos);
    }

    child->last_range(last_range());
    child->first_range(split_range);

    last_range(split_range->prev());
    if (last_range() != NULL) last_range()->next(NULL);
    if (last_range() == NULL || split_range->prev()->prev() == NULL) {
      first_range(last_range());
    }

    split_range->prev(NULL);
  }

  // Split uses
  LIRUse* split_use = last_use();
  while (split_use != NULL && split_use->pos()->id() >= pos &&
         split_use->prev() != NULL && split_use->pos()->id() < pos) {
    split_use = split_use->prev();
  }

  if (split_use != NULL && split_use->pos()->id() >= pos) {
    child->last_use(last_use());
    child->first_use(split_use);

    last_use(split_use->prev());
    if (last_use() == NULL) {
      first_use(last_use());
    } else {
      last_use()->next(NULL);
    }

    split_use->prev(NULL);
  }

  return child;
}


LIRInterval* LIRInterval::ChildAt(int pos) {
  if (Covers(pos)) return this;

  LIRIntervalList::Item* item = children()->head();
  for (; item != NULL; item = item->next()) {
    if (item->value()->Covers(pos)) return item->value()->ChildAt(pos);
  }

  return NULL;
}


int LIRIntervalShape::Compare(LIRInterval* a, LIRInterval* b) {
  return a->start() - b->start();
}


void LIRInterval::AddLiveRange(int start, int end) {
  assert(start < end);

  if (first_range() == NULL) {
    first_range(new LIRLiveRange(start, end));
    last_range(first_range());
  } else if (start >= first_range()->start()) {
    // Ignore
  } else if (end == first_range()->start()) {
    // Concat ranges
    first_range()->start(start);
  } else {
    LIRLiveRange* r = new LIRLiveRange(start, end);
    r->next(first_range());
    first_range()->prev(r);
    first_range(r);
  }
}


void LIRInterval::AddUse(LIRInstruction* pos, LIROperand::Type kind) {
  LIRUse* use = new LIRUse(pos, kind);
  if (first_use() == NULL) {
    first_use(use);
    last_use(use);
  } else {
    first_use()->prev(use);
    use->next(first_use());
    first_use(use);
  }
}


bool LIRInterval::Covers(int pos) {
  LIRLiveRange* range = first_range();
  for (; range != NULL; range = range->next()) {
    // All other ranges are starting after pos
    if (range->start() > pos) return false;

    // Range ends before pos
    if (range->end() <= pos) continue;

    return true;
  }
  return false;
}


int LIRInterval::FindIntersection(LIRInterval* interval) {
  LIRLiveRange* a = this->first_range();
  for (; a != NULL; a = a->next()) {
    LIRLiveRange* b = interval->first_range();
    for (; b != NULL; b = b->next()) {
      int pos = a->FindIntersection(b);
      if (pos != -1) return pos;
    }
  }

  return -1;
}


LIRUse* LIRInterval::NextUseAfter(int pos) {
  LIRUse* use = first_use();
  for (; use != NULL && use->pos()->id() < pos; use = use->next()) {
    // Intentionally blank
  }

  if (use != NULL) return use;

  LIRIntervalList::Item* item = children()->head();
  for (; item != NULL; item = item->next()) {
    use = item->value()->NextUseAfter(pos);
    if (use != NULL) return use;
  }

  return NULL;
}


void LIRInterval::SplitAndSpill(LIRAllocator* allocator,
                                LIRInterval* interval) {
  int pos = FindIntersection(interval);

  // If no intersection was found - no spilling is required
  if (pos == -1) return;

  // Intervals intersects at `interval` start point
  if (pos <= this->start()) {
    allocator->AssignSpill(interval);
    return;
  }

  LIRInterval* child = SplitAt(pos);
  allocator->AssignSpill(child);
  allocator->AddUnhandled(child);

  HIRInstruction* hinstr = allocator->blocks()->tail()->value()
      ->FindInstruction(pos);
  if (hinstr != hinstr->block()->first_instruction()) {
    HIRParallelMove::GetBefore(hinstr)->AddMove(this, child);
  }
}


LIROperand* LIRInterval::OperandAt(int pos) {
  LIRInterval* current = this;

  while (current != NULL && !current->Covers(pos)) {
    LIRIntervalList::Item* item = current->children()->head();

    // Find appropriate child
    while (item != NULL && item->value()->rec_start() < pos) {
      if (item->value()->rec_end() > pos) break;
      item = item->next();
    }

    if (item != NULL) {
      current = item->value();
    } else {
      current = NULL;
    }
  }

  return current != NULL ? current->operand() : NULL;
}


void LIRValue::ReplaceWithOperand(LIRInstruction* instr, LIROperand** operand) {
  LIROperand* original = *operand;
  if (original->is_virtual()) {
    *operand = LIRValue::Cast(original)->interval()->OperandAt(instr->id());
  } else if ((*operand)->is_interval()) {
    *operand = LIRInterval::Cast(original)->OperandAt(instr->id());
  }
  assert(*operand != NULL);
}


void LIRAllocator::Init() {
  ComputeLocalLiveSets();
  ComputeGlobalLiveSets();

  for (int i = 0; i < kLIRRegisterCount; i++) {
    registers()[i] = new LIRValue(NULL);
    registers()[i]->interval()->kind(LIRInterval::kFixed);
    registers()[i]->interval()
        ->operand(new LIROperand(LIROperand::kRegister, i));
  }

  BuildIntervals();

  WalkIntervals();
  ResolveDataFlow();
}


void LIRAllocator::ComputeLocalLiveSets() {
  HIRBasicBlockList::Item* block_item = blocks()->tail();
  for (; block_item != NULL; block_item = block_item->prev()) {
    HIRBasicBlock* block = block_item->value();
    HIRInstructionList::Item* instr = block->instructions()->head();
    for (; instr != NULL; instr = instr->next()) {
      // Process inputs
      HIRValueList::Item* item = instr->value()->values()->head();
      for (; item != NULL; item = item->next()) {
        // XXX: Use hashmaps here
        if (item->value()->IsIn(block->live_kill()) ||
            item->value()->IsIn(block->live_gen())) {
          continue;
        }
        block->live_gen()->Push(item->value());
      }

      // Process result
      if (instr->value()->GetResult() != NULL) {
        HIRValue* result = instr->value()->GetResult();

        // XXX: Use hashmaps here
        if (!result->IsIn(block->live_kill())) {
          block->live_kill()->Push(result);
        }
      }
    }
  }
}


void LIRAllocator::ComputeGlobalLiveSets() {
  HIRBasicBlockList::Item* block_item = blocks()->tail();
  // This traverse SHOULD be bottom-up
  for (; block_item != NULL; block_item = block_item->prev()) {
    HIRBasicBlock* block = block_item->value();
    HIRValueList::Item* item;

    // Propagate inputs from children to parent
    for (int i = 0; i < block->successor_count(); i++) {
      item = block->successors()[i]->live_in()->head();
      // XXX: Use hashmaps here
      for (; item != NULL; item = item->next()) {
        if (item->value()->IsIn(block->live_out())) continue;
        block->live_out()->Push(item->value());
      }
    }

    // Propagate definitions to inputs
    item = block->live_gen()->head();
    for (; item != NULL; item = item->next()) {
      block->live_in()->Push(item->value());
    }

    // That ain't defined in block
    item = block->live_out()->head();
    for (; item != NULL; item = item->next()) {
      // XXX: Use hashmaps here
      if (item->value()->IsIn(block->live_kill()) ||
          item->value()->IsIn(block->live_in())) {
        continue;
      }
      block->live_in()->Push(item->value());
    }
  }
}


void LIRAllocator::BuildIntervals() {
  HIRBasicBlockList::Item* block_item = blocks()->tail();
  for (; block_item != NULL; block_item = block_item->prev()) {
    HIRBasicBlock* block = block_item->value();
    assert(block->instructions()->length() != 0);

    int from = block->first_instruction()->id();
    int to = block->last_instruction()->id() + 2;

    // Process block's outputs
    {
      HIRValueList::Item* item = block->live_out()->head();
      for (; item != NULL; item = item->next()) {
        LIRValue* value = item->value()->lir();

        value->interval()->AddLiveRange(from, to);
        AddUnhandled(value->interval());
      }
    }

    // Process block's instructions in reverse order
    {
      HIRInstructionList::Item* item = block->instructions()->tail();
      for (; item != NULL; item = item->prev()) {
        HIRInstruction* hinstr = item->value();
        LIRInstruction* linstr = hinstr->lir(lir());

        // Add 1-range to every available register
        if (hinstr->HasCall()) {
          for (int i = 0; i < kLIRRegisterCount; i++) {
            LIRValue* fixed = new LIRValue(NULL);

            fixed->interval()->AddLiveRange(linstr->id(), linstr->id() + 1);
            fixed->interval()->AddUse(linstr, LIROperand::kRegister);
            fixed->interval()->kind(LIRInterval::kFixed);
            fixed->interval()->operand(registers()[i]->interval()->operand());

            AddUnhandled(fixed->interval());
          }
        }

        // Process output
        if (hinstr->GetResult() != NULL) {
          LIRValue* result = hinstr->GetResult()->lir();

          // Shorten range or create small one
          if (result->interval()->first_range() == NULL) {
            result->interval()->AddLiveRange(linstr->id(), linstr->id() + 2);
          } else {
            result->interval()->first_range()->start(linstr->id());
          }
          result->interval()->AddUse(linstr, LIROperand::kRegister);
          AddUnhandled(result->interval());

          if (result->interval()->end() > linstr->id() + 2) {
            linstr->result = GetFixed(kFixedAfter,
                                      linstr,
                                      result,
                                      linstr->result);
          } else {
            // If result is not-used after definition - fix it
            result->interval()->AddUse(linstr, LIROperand::kRegister);
            result->interval()->kind(LIRInterval::kFixed);

            linstr->result = result;
          }
        }

        // Process scratches
        for (int i = 0; i < linstr->scratch_count(); i++) {
          LIRValue* value = new LIRValue(NULL);
          linstr->scratches[i] = value;
          value->interval()->AddLiveRange(linstr->id(), linstr->id() + 1);
          value->interval()->AddUse(linstr, LIROperand::kRegister);
          value->interval()->kind(LIRInterval::kFixed);
          AddUnhandled(value->interval());
        }

        // Process inputs
        HIRValueList::Item* item = hinstr->values()->head();
        assert(hinstr->values()->length() >= linstr->input_count());
        for (int i = 0; i < linstr->input_count(); item = item->next(), i++) {
          LIRValue* value = item->value()->lir();

          if (linstr->inputs[i] == NULL) {
            value->interval()->AddUse(linstr, LIROperand::kVirtual);
            linstr->inputs[i] = value;
          } else {
            value->interval()->AddUse(linstr, LIROperand::kRegister);
            GetFixed(kFixedBefore,
                     linstr,
                     value,
                     linstr->inputs[i]);
          }
          value->interval()->AddLiveRange(from, linstr->id() + 1);
          AddUnhandled(value->interval());
        }
      }
    }
  }

  unhandled()->Sort<LIRIntervalShape>();
}


void LIRAllocator::WalkIntervals() {
  while (unhandled()->length() > 0) {
    LIRInterval* current = unhandled()->Shift();
    int position = current->start();

    // Walk active intervals and move/remove them if needed
    LIRIntervalList::Item* item;
    for (item = active()->head(); item != NULL; item = item->next()) {
      if (item->value()->end() <= position) {
        // Remove expired interval
        active()->Remove(item);
        if (item->value()->operand()->is_spill()) {
          // And add it to the available list
          ReleaseSpill(item->value()->operand());
        }
      } else if (!item->value()->Covers(position)) {
        // Move interval to inactive
        inactive()->Push(item->value());
        active()->Remove(item);
      }
    }

    // Walk inactive ones
    for (item = inactive()->head(); item != NULL; item = item->next()) {
      if (item->value()->end() <= position) {
        // Remove expired interval
        inactive()->Remove(item);
        if (item->value()->operand()->is_spill()) {
          // And add it to the available list
          ReleaseSpill(item->value()->operand());
        }
      } else if (item->value()->Covers(position)) {
        // Move interval to active
        active()->Push(item->value());
        inactive()->Remove(item);
      }
    }

    if (current->operand() == NULL) {
      // Allocate register for the current interval
      if (!AllocateFreeReg(current)) {
        AllocateBlockedReg(current);
      }
    } else if (current->is_fixed()) {
      SplitAndSpillIntersecting(current);
    }

    assert(current->operand() != NULL);
    if (current->operand()->is_register() ||
        current->operand()->is_spill()) {
      active()->Push(current);
    }
  }
}


bool LIRAllocator::AllocateFreeReg(LIRInterval* interval) {
  int free_pos[kLIRRegisterCount];

  for (int i = 0; i < kLIRRegisterCount; i++) {
    free_pos[i] = INT_MAX;
  }

  // Set free_pos for all active registers
  LIRIntervalList::Item* item;
  for (item = active()->head(); item != NULL; item = item->next()) {
    if (!item->value()->operand()->is_register()) continue;

    assert(item->value()->operand() != NULL);
    free_pos[item->value()->operand()->value()] = 0;
  }

  // Set free_pos for all inactive registers that intersects with current
  // interval
  for (item = inactive()->head(); item != NULL; item = item->next()) {
    if (!item->value()->operand()->is_register()) continue;

    int pos = item->value()->FindIntersection(interval);
    if (pos == -1) continue;

    assert(item->value()->operand() != NULL);
    free_pos[item->value()->operand()->value()] = pos;
  }

  // Find register with a maximum free_pos
  int max = free_pos[0];
  int max_i = 0;
  for (int i = 0; i < kLIRRegisterCount; i++) {
    if (free_pos[i] > max) {
      max = free_pos[i];
      max_i = i;
    }
  }

  // Allocation failed, spill is needed
  if (max == 0) return false;

  if (max < interval->end()) {
    // XXX: Choose optimal split position
    LIRInterval* split_child = interval->SplitAt(max);
    AddUnhandled(split_child);

    // Create movement if instruction wasn't on the block edge
    HIRInstruction* hinstr = blocks()->tail()->value()->FindInstruction(max);
    if (hinstr != hinstr->block()->first_instruction()) {
      HIRParallelMove::GetBefore(hinstr)->AddMove(interval, split_child);
    }
  } else {
    // Register is available for the whole interval's lifetime
    // (Intentionally left empty)
  }

  // Assign register to the interval
  assert(interval->operand() == NULL);
  interval->operand(registers()[max_i]->interval()->operand());

  return true;
}


void LIRAllocator::AllocateBlockedReg(LIRInterval* interval) {
  int use_pos[kLIRRegisterCount];
  int block_pos[kLIRRegisterCount];

  for (int i = 0; i < kLIRRegisterCount; i++) {
    use_pos[i] = INT_MAX;
    block_pos[i] = INT_MAX;
  }

  // Process active intervals
  LIRIntervalList::Item* item;
  for (item = active()->head(); item != NULL; item = item->next()) {
    if (!item->value()->operand()->is_register()) continue;

    assert(item->value()->operand() != NULL);
    int reg_num = item->value()->operand()->value();

    if (item->value()->is_fixed()) {
      block_pos[reg_num] = 0;
    } else {
      LIRUse* use = item->value()->NextUseAfter(interval->start());
      use_pos[reg_num] = use->pos()->id();
    }
  }

  // Process inactive ones
  for (item = inactive()->head(); item != NULL; item = item->next()) {
    if (!item->value()->operand()->is_register()) continue;

    int pos = item->value()->FindIntersection(interval);
    if (pos == -1) continue;

    assert(item->value()->operand() != NULL);
    int reg_num = item->value()->operand()->value();

    if (item->value()->is_fixed()) {
      block_pos[reg_num] = pos;
    } else {
      use_pos[reg_num] =
          item->value()->NextUseAfter(interval->start())->pos()->id();
    }
  }

  int max_use = use_pos[0];
  int max_i = 0;
  for (int i = 0; i < kLIRRegisterCount; i++) {
    if (use_pos[i] > max_use) {
      max_use = use_pos[i];
      max_i = i;
    }
  }

  if (!interval->is_fixed()) {
    if (max_use < interval->NextUseAfter(0)->pos()->id() ||
        block_pos[max_i] < interval->end()) {
      AssignSpill(interval);
      return;
    } else if (block_pos[max_i] <= interval->end()) {
      // Split current interval at optimal position
      AddUnhandled(interval->SplitAt(block_pos[max_i]));
    }

    assert(interval->operand() == NULL);

    // Assign register to interval
    interval->operand(registers()[max_i]->interval()->operand());
  } else {
    if (interval->operand() == NULL) {
      interval->operand(registers()[max_i]->interval()->operand());
    }
  }

  SplitAndSpillIntersecting(interval);
}


void LIRAllocator::SplitAndSpillIntersecting(LIRInterval* interval) {
  // Split and spill every active/inactive interval with that register
  // that intersects with current interval.
  int pos;
  LIRIntervalList* lists[2] = { active(), inactive() };
  for (int i = 0; i < 2; i++) {
    LIRIntervalList::Item* item;
    for (item = lists[i]->head(); item != NULL; item = item->next()) {
      if ((item->value() == interval) ||
          !item->value()->operand()->is_register() ||
          !item->value()->operand()->is_equal(interval->operand())) {
        continue;
      }
      item->value()->SplitAndSpill(this, interval);
    }
  }
}


void LIRAllocator::ResolveDataFlow() {
  HIRBasicBlockList::Item* block_item = blocks()->tail();
  for (; block_item != NULL; block_item = block_item->prev()) {
    HIRBasicBlock* from = block_item->value();
    HIRInstructionList::Item* instr = from->instructions()->head();
    HIRParallelMove* move = reinterpret_cast<HIRParallelMove*>(
        from->last_instruction()->prev());

    for (int i = 0; i < from->successor_count(); i++) {
      HIRBasicBlock* to = from->successors()[i];

      HIRValueList::Item* item = to->live_in()->head();
      for (; item != NULL; item = item->next()) {
        LIRInterval* parent = item->value()->lir()->interval();
        LIRInterval* from_interval = parent->ChildAt(
            from->last_instruction()->id());
        LIRInterval* to_interval = parent->ChildAt(
            to->first_instruction()->id());

        if (from_interval == to_interval) continue;

        // Lazy-create move
        if (move == NULL || move->type() != HIRInstruction::kParallelMove) {
          move = HIRParallelMove::GetBefore(from->last_instruction());
        }

        move->AddMove(from_interval->operand(), to_interval->operand());
      }
    }
  }
}


LIRInterval* LIRAllocator::GetFixed(FixedPosition position,
                                    LIRInstruction* instr,
                                    LIRValue* value,
                                    LIROperand* operand) {
  LIRInterval* fixed;

  if (position == kFixedBefore) {
    // value: ------------
    // use:        [ ]
    // after:         ----
    // fixed:      ---
    // left:  -----
    LIRInterval* after = value->interval()->SplitAt(instr->id() + 2);
    AddUnhandled(after);
    fixed = value->interval()->SplitAt(instr->id());
    AddUnhandled(fixed);

    HIRParallelMove::GetBefore(instr->generic_hir())->AddMove(value->interval(),
                                                              fixed);
    HIRParallelMove::GetAfter(instr->generic_hir())->AddMove(fixed, after);
  } else {
    LIRInterval* after = value->interval()->SplitAt(instr->id() + 2);
    fixed = value->interval();
    AddUnhandled(after);
    AddUnhandled(fixed);

    HIRParallelMove::GetAfter(instr->generic_hir())->AddMove(fixed, after);
  }

  fixed->AddUse(instr, LIROperand::kRegister);
  fixed->kind(LIRInterval::kFixed);

  if (operand != NULL) fixed->operand(operand);

  return fixed;
}

} // namespace internal
} // namespace candor
