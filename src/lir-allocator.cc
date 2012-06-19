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
  LIRInterval* child = new LIRInterval(value());

  child->parent(this);

  this->children()->Unshift(child);

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
    if (last_range() == NULL) {
      first_range(last_range());
    } else {
      last_range()->next(NULL);
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

  return use;
}


LIRInterval* LIRInterval::GetFixed(LIRInstruction* instr, LIROperand* value) {
  LIRInterval* interval = SplitAt(instr->id());

  interval->kind(LIRInterval::kFixed);
  interval->operand(value);
  interval->AddLiveRange(instr->id(), instr->id() + 1);
  interval->AddUse(instr, LIROperand::kRegister);

  return interval;
}


LIRInterval* LIRInterval::SplitAndSpill(LIRAllocator* allocator,
                                        LIRInterval* interval) {
  int pos = FindIntersection(interval);

  // If no intersection was found - no spilling is required
  if (pos == -1) return NULL;

  LIRInterval* child = SplitAt(pos);
  allocator->AssignSpill(child);
  allocator->AddUnhandled(child);

  return child;
}


LIRInterval* LIRValue::FindInterval(int pos) {
  LIRInterval* current = interval();

  while (current != NULL && (current->start() < pos || current->end() >= pos)) {
    LIRIntervalList::Item* item = current->children()->head();

    // Find appropriate child
    for (; item != NULL && item->value()->start() < pos; item = item->next()) {
      // Loop's body is intentionally empty
    }

    if (item != NULL) {
      current = item->value();
    } else {
      current = NULL;
    }
  }

  return current;
}


void LIRAllocator::Init(HIRBasicBlock* block) {
  ComputeLocalLiveSets(block);
  ComputeGlobalLiveSets(block);

  for (int i = 0; i < kLIRRegisterCount; i++) {
    registers()[i] = new LIRValue(NULL);
    registers()[i]->interval()->kind(LIRInterval::kFixed);
    registers()[i]->interval()
        ->operand(new LIROperand(LIROperand::kRegister, i));
    AddUnhandled(registers()[i]->interval());
  }

  BuildIntervals(block);
  WalkIntervals();
  ResolveDataFlow(block);
}


void LIRAllocator::ComputeLocalLiveSets(HIRBasicBlock* block) {
  for (; block != NULL; block = block->prev()) {
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

    if (block->prev() != NULL && block->prev()->predecessor_count() == 0) {
      break;
    }
  }
}


void LIRAllocator::ComputeGlobalLiveSets(HIRBasicBlock* block) {
  // This traverse SHOULD be bottom-up
  for (; block != NULL; block = block->prev()) {
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

    if (block->prev() != NULL && block->prev()->predecessor_count() == 0) {
      break;
    }
  }
}


void LIRAllocator::BuildIntervals(HIRBasicBlock* block) {
  for (; block != NULL; block = block->prev()) {
    assert(block->instructions()->length() != 0);

    int from = block->first_instruction()->id();
    int to = block->last_instruction()->id();

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
            registers()[i]->interval()
                ->AddLiveRange(linstr->id(), linstr->id() + 1);
          }
        }

        // Process output
        if (hinstr->GetResult() != NULL) {
          LIRValue* result = hinstr->GetResult()->lir();

          assert(result->interval()->first_range() != NULL);
          result->interval()->first_range()->start(linstr->id() + 1);
          result->interval()->AddUse(linstr, LIROperand::kRegister);
          AddUnhandled(result->interval());

          // Split interval to create fixed
          AddUnhandled(result->interval()->GetFixed(linstr, linstr->result));

          linstr->result = result;
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
          } else {
            // Shift live ranges a bit so they won't intersect:
            // fixed:          -
            // previous:        ------
            if (value->interval()->first_range() != NULL &&
                value->interval()->first_range()->start() <= linstr->id()) {
              value->interval()->first_range()->start(linstr->id() + 1);
            }

            // Create fixed interval
            AddUnhandled(value->interval()->GetFixed(linstr,
                                                     linstr->inputs[i]));
          }
          value->interval()->AddLiveRange(from, linstr->id());

          linstr->inputs[i] = value;
        }
      }
    }

    // Stop on entry block
    if (block->prev() != NULL && block->prev()->predecessor_count() == 0) break;
  }
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
      } else if (!item->value()->Covers(position)) {
        // Move interval to inactive
        inactive()->Push(item->value());
        active()->Remove(item);
      }
    }

    // Walk active spills
    for (item = active_spills()->head(); item != NULL; item = item->next()) {
      if (item->value()->end() <= position) {
        // Remove expired spilled interval
        active_spills()->Remove(item);
        // And add it to the available list
        available_spills()->Push(item->value()->operand());
      } else if (!item->value()->Covers(position)) {
        // Move interval to inactive
        inactive()->Push(item->value());
        active_spills()->Remove(item);
      }
    }

    // Walk inactive ones
    for (item = inactive()->head(); item != NULL; item = item->next()) {
      if (item->value()->end() <= position) {
        // Remove expired interval
        inactive()->Remove(item);
      } else if (item->value()->Covers(position)) {
        // Move interval to active
        if (item->value()->operand()->is_register()) {
          active()->Push(item->value());
        }
        inactive()->Remove(item);
      }
    }

    if (current->operand() == NULL) {
      // Allocate register for the current interval
      if (!AllocateFreeReg(current)) {
        AllocateBlockedReg(current);
      }
    } else {
      assert(current->is_fixed());
    }

    assert(current->operand() != NULL);
    if (current->operand()->is_register()) {
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
    assert(item->value()->operand() != NULL);
    free_pos[item->value()->operand()->value()] = 0;
  }

  // Set free_pos for all inactive registers that intersects with current
  // interval
  for (item = inactive()->head(); item != NULL; item = item->next()) {
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
    AddUnhandled(interval->SplitAt(max));
  } else {
    // Register is available for the whole interval's lifetime
    // (Intentionally left empty)
  }

  // Assign register to the interval
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
    assert(item->value()->operand() != NULL);
    int reg_num = item->value()->operand()->value();

    if (item->value()->is_fixed()) {
      block_pos[reg_num] = 0;
    } else {
      use_pos[reg_num] =
          item->value()->NextUseAfter(interval->start())->pos()->id();
    }
  }

  // Process inactive ones
  for (item = inactive()->head(); item != NULL; item = item->next()) {
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

  if (max_use < interval->first_use()->pos()->id()) {
    AssignSpill(interval);
    return;
  } else if (block_pos[max_i] <= interval->end()) {
    // Split current interval at optimal position
    AddUnhandled(interval->SplitAt(block_pos[max_i]));
  }

  // Assign register to interval
  interval->operand(registers()[max_i]->interval()->operand());

  // Split and spill every active/inactive interval with that register
  // that intersects with current interval.
  for (item = active()->head(); item != NULL; item = item->next()) {
    if (!item->value()->operand()->is_equal(interval->operand())) continue;
    item->value()->SplitAndSpill(this, interval);
  }

  for (item = inactive()->head(); item != NULL; item = item->next()) {
    if (!item->value()->operand()->is_equal(interval->operand())) continue;
    item->value()->SplitAndSpill(this, interval);
  }
}


void LIRAllocator::ResolveDataFlow(HIRBasicBlock* block) {
}

} // namespace internal
} // namespace candor
