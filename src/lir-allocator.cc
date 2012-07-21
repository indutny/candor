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
  assert(pos >= start());

  if (pos <= start()) return NULL;

  // Position points to children's ranges
  if (pos >= end()) {
    LIRIntervalList::Item* item = children()->tail();
    for (; item != NULL; item = item->prev()) {
      if (item->value()->start() >= pos) return item->value()->SplitAt(pos);
    }
  }

  // Split ranges
  LIRLiveRange* split_range = last_range();
  while (split_range != NULL && split_range->end() >= pos &&
         split_range->prev() != NULL && split_range->prev()->end() < pos) {
    split_range = split_range->prev();
  }

  // Split can't be performed (pos is on the interval's edge, or behind it)
  if (split_range == NULL || split_range->end() < pos) return NULL;

  // Position is inside current interval ranges
  LIRInterval* child = new LIRInterval(value());

  child->parent(this);
  this->children()->InsertSorted<LIRIntervalShape>(child);


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

  // Split uses
  LIRUse* split_use = last_use();
  while (split_use != NULL && split_use->pos()->id() >= pos &&
         split_use->prev() != NULL && split_use->prev()->pos()->id() >= pos) {
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

  return current;
}


int LIRIntervalShape::Compare(LIRInterval* a, LIRInterval* b) {
  if (a->start() != b->start() || (a->is_fixed() && b->is_fixed())) {
    return a->start() - b->start();
  } else {
    // Fixed intervals should come first
    return a->is_fixed() ? -1 : 1;
  }
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


LIRUse* LIRInterval::AddUse(LIRInstruction* pos, LIROperand::Type kind) {
  LIRUse* use = new LIRUse(pos, kind);
  if (first_use() == NULL) {
    first_use(use);
    last_use(use);
  } else {
    first_use()->prev(use);
    use->next(first_use());
    first_use(use);
  }

  return use;
}


bool LIRInterval::Covers(int pos) {
  LIRLiveRange* range = first_range();
  for (; range != NULL; range = range->next()) {
    // All other ranges are starting after pos
    if (range->start() > pos) return false;

    if (range->end() > pos) return true;
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


LIRUse* LIRInterval::NextUseAfter(LIROperand::Type kind, int pos) {
  LIRUse* use = first_use();
  for (; use != NULL && use->pos()->id() < pos; use = use->next()) {
    if (kind != LIROperand::kAny && use->kind() != kind) continue;
  }

  if (use != NULL) return use;

  LIRIntervalList::Item* item = children()->head();
  for (; item != NULL; item = item->next()) {
    use = item->value()->NextUseAfter(kind, pos);
    if (use != NULL) return use;
  }

  return NULL;
}


void LIRInterval::SplitAndSpill(LIRAllocator* allocator,
                                LIRInterval* interval) {
  int pos = FindIntersection(interval);

  // If no intersection was found - no spilling is required
  if (pos == -1) return;

  // Do not spill things that ain't used after
  if (pos + 1 >= this->end()) return;

  // Intervals intersects at `interval` start point
  if (pos <= this->start()) {
    allocator->AssignSpill(interval);
    return;
  }

  LIRInterval* child = SplitAt(pos);
  if (child == NULL) return;

  allocator->AddMoveBefore(this->end(), this->last_use(), child->first_use());
  allocator->AddUnhandled(child);
}


LIROperand* LIRInterval::OperandAt(int pos, bool is_move) {
  LIRInterval* current = this;
  bool align = 0;

  // If interval's start is odd and pos is right before it - align pos
  if (is_move) {
    if (pos + 1 == start()) {
      pos = pos + 1;
      align = 1;
    } else if (pos - 1 == start()) {
      pos = pos - 1;
      align = -1;
    }
  }

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

  if (current == NULL) return NULL;
  if (is_move) return current->operand();

  // Return use's operand in case if we have any at this position
  pos = pos - align;
  LIRUse* use = current->NextUseAfter(LIROperand::kRegister, pos);
  if (use != NULL && use->pos()->id() == pos && use->operand() != NULL) {
    return use->operand();
  }

  return current->operand();
}


void LIRValue::ReplaceWithOperand(LIRInstruction* instr,
                                  LIROperand** operand,
                                  bool is_move) {
  LIROperand* original = *operand;
  LIRInterval* interval = NULL;
  if (!original->is_use()) {
    if (original->is_virtual()) {
      interval= LIRValue::Cast(original)->interval();
    } else if ((*operand)->is_interval()) {
      interval = LIRInterval::Cast(original);
    }

    if (interval != NULL) {
      *operand = interval->OperandAt(instr->id(), is_move);
    }
  } else {
    *operand = LIRUse::Cast(original)->operand();
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
  // ResolveDataFlow();
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
            result->interval()->AddLiveRange(linstr->id() - 1,
                                             linstr->id() + 1);
          } else {
            result->interval()->first_range()->start(linstr->id() - 1);
          }
          LIRUse* use = result->interval()->AddUse(linstr,
                                                   LIROperand::kRegister);
          use->operand(linstr->result);
          AddUnhandled(result->interval());

          if (result->interval()->end() > linstr->id() + 1) {
            GetFixed(kFixedAfter, linstr, result, linstr->result);
          }
          linstr->result = use;
        }

        // Process scratches
        for (int i = 0; i < linstr->scratch_count(); i++) {
          LIRValue* value = new LIRValue(NULL);

          // Scratch should be live slightly before instruction,
          // otherwise it won't have a register assigned to it if instruction
          // HasCall()
          value->interval()->AddLiveRange(linstr->id() - 1, linstr->id() + 1);
          linstr->scratches[i] =
              value->interval()->AddUse(linstr, LIROperand::kRegister);
          AddUnhandled(value->interval());
        }

        // Process inputs
        HIRValueList::Item* item = hinstr->values()->head();
        assert(hinstr->values()->length() >= linstr->input_count());
        for (int i = 0; i < linstr->input_count(); item = item->next(), i++) {
          LIRValue* value = item->value()->lir();

          value->interval()->AddLiveRange(from, linstr->id() + 1);
          if (linstr->inputs[i] == NULL) {
            linstr->inputs[i] =
                value->interval()->AddUse(linstr, LIROperand::kAny);
          } else {
            value->interval()->AddUse(linstr, LIROperand::kRegister);
            GetFixed(kFixedBefore,
                     linstr,
                     value,
                     linstr->inputs[i]);
          }
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
      AllocateReg(current);
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


void LIRAllocator::AllocateReg(LIRInterval* interval) {
  if (!AllocateFreeReg(interval)) {
    AllocateBlockedReg(interval);
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
    if (split_child != NULL) {
      AddUnhandled(split_child);

      // Create movement if instruction wasn't on the block edge
      AddMoveBefore(max, interval->last_use(), split_child->first_use());
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
      LIRUse* use = item->value()->NextUseAfter(LIROperand::kRegister,
                                                interval->start());
      if (use == NULL) continue;
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
      LIRUse* use = item->value()->NextUseAfter(LIROperand::kRegister,
                                                interval->start());
      if (use == NULL) continue;
      use_pos[reg_num] = use->pos()->id();
    }
  }

  int max_use = -1;
  int max_i = -1;
  for (int i = 0; i < kLIRRegisterCount; i++) {
    if (block_pos[i] < use_pos[i]) use_pos[i] = block_pos[i];
    if (use_pos[i] > max_use) {
      max_use = use_pos[i];
      max_i = i;
    }
  }
  assert(max_i != -1);

  if (!interval->is_fixed()) {
    // All active and inactive intervals are used before current
    if (max_use < interval->start()) {
      LIRUse* first_use = interval->NextUseAfter(LIROperand::kRegister,
                                                 0);
      if (first_use != NULL) {
        LIRUse* next_use = interval->NextUseAfter(LIROperand::kRegister,
                                                  first_use->pos()->id() + 1);

        // Better split and spill current
        if (next_use != NULL && next_use->pos()->id() < interval->end()) {
          // Add some padding before for HasCall() cases
          LIRInterval* after = interval->SplitAt(next_use->pos()->id() - 1);
          if (after != NULL) {
            AddUnhandled(after);
            AddMoveBefore(after->start() + 1,
                          interval->last_use(),
                          next_use);
          }
        }
        LIROperand* operand = interval->parent()->OperandAt(
            first_use->pos()->id() - 1);
        first_use->operand(operand);
      }
      AssignSpill(interval);
      return;
    } else if (block_pos[max_i] <= interval->end()) {
      // Split current interval at optimal position before block pos
      LIRInterval* after = interval->SplitAt(block_pos[max_i]);

      AddUnhandled(after);
      AddMoveBefore(block_pos[max_i], interval->last_use(), after->first_use());
    }

    assert(interval->operand() == NULL);

    // Assign register to interval
    interval->operand(registers()[max_i]->interval()->operand());
  } else {
    // Fixed intervals should be already filled!
    assert(interval->operand() != NULL);
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
          item->value()->is_fixed() ||
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
    // after:         ?----
    // fixed:      ---
    // left:  -----
    LIRInterval* after = value->interval()->SplitAt(instr->id() + 2);
    AddUnhandled(after);
    fixed = value->interval()->SplitAt(instr->id());
    AddUnhandled(fixed);

    assert(fixed != NULL);
    AddMoveBefore(instr->id(),
                  value->interval()->last_use(),
                  fixed->first_use());

    // Extra shuffle is needed to put spilled register to the requested reg
    if (instr->generic_hir()->HasCall() && operand != NULL) {
      AddMoveBefore(instr->id(), value->interval()->last_use(), operand);
    }
    if (after != NULL) {
      AddMoveBefore(instr->id() + 2, fixed->last_use(), after->first_use());
    }
  } else {
    LIRInterval* after = value->interval()->SplitAt(instr->id() + 2);
    fixed = value->interval();
    AddUnhandled(after);
    AddUnhandled(fixed);

    if (after != NULL) {
      AddMoveBefore(instr->id() + 2, fixed->last_use(), after->first_use());
    }
  }

  LIRUse* use = fixed->AddUse(instr, LIROperand::kRegister);

  if (operand != NULL) use->operand(operand);

  return fixed;
}

} // namespace internal
} // namespace candor
