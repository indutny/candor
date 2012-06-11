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

#include <assert.h>

namespace candor {
namespace internal {

LIRInterval* LIRInterval::SplitAt(int pos) {
  assert(start() < pos && pos < end());

  LIRInterval* child = new LIRInterval(value());

  child->parent(this);
  child->operand(this->operand());

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


void LIRInterval::AddUse(LIRInstruction* pos,
                         LIROperand::Type kind,
                         LIROperand* operand) {
  LIRUse* use = new LIRUse(pos, kind, operand);
  if (first_use() == NULL) {
    first_use(use);
    last_use(use);
  } else {
    first_use()->prev(use);
    use->next(first_use());
    first_use(use);
  }
}


LIRInterval* LIRValue::FindInterval(int pos) {
  LIRInterval* current = interval();

  while (current != NULL && (current->start() < pos || current->end() >= pos)) {
    LIRIntervalList::Item* item = current->children()->head();

    // Find appropriate child
    for (; item != NULL && item->value()->start() < pos; item = item->next()) {
    }

    if (item != NULL) {
      current = item->value();
    } else {
      current = NULL;
    }
  }

  return current;
}


void LIRAllocator::Init() {
  for (int i = 0; i < kLIRRegisterCount; i++) {
    registers()[i] = new LIRValue(NULL);
    registers()[i]->interval()
        ->operand(new LIROperand(LIROperand::kRegister, i));
  }

  BuildInstructions();
  BuildIntervals();
}


void LIRAllocator::BuildInstructions() {
  HIRInstruction* hinstr = hir()->first_instruction();

  for (; hinstr != NULL; hinstr = hinstr->next()) {
    LIRInstruction* linstr = hinstr->lir(lir());
    lir()->AddInstruction(linstr);
  }
}


void LIRAllocator::BuildIntervals() {
  HIRBasicBlockList::Item* item = hir()->enumerated_blocks()->tail();
  for (; item != NULL; item = item->prev()) {
    HIRBasicBlock* block = item->value();
    assert(block->instructions()->length() != 0);

    int from = block->first_instruction()->id();
    int to = block->last_instruction()->id();

    // Process block's outputs
    {
      HIRValueList::Item* item;
      for (item = block->values()->head(); item != NULL; item = item->next()) {
        LIRValue* value = item->value()->lir();

        value->interval()->AddLiveRange(from, to);
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
          result->interval()->first_range()->start(linstr->id());
          result->interval()->AddUse(linstr,
                                     LIROperand::kRegister,
                                     linstr->result);

          linstr->result = result;
        }

        // Process scratches
        for (int i = 0; i < linstr->scratch_count(); i++) {
          LIRValue* value = new LIRValue(NULL);
          linstr->scratches[i] = value;
          value->interval()->AddLiveRange(linstr->id(), linstr->id() + 1);
          value->interval()->AddUse(linstr, LIROperand::kRegister, NULL);
        }

        // Process inputs
        HIRValueList::Item* item = hinstr->values()->head();
        assert(hinstr->values()->length() >= linstr->input_count());
        for (int i = 0; i < linstr->input_count(); item = item->next(), i++) {
          LIRValue* value = item->value()->lir();
          value->interval()->AddLiveRange(from, linstr->id());
          value->interval()->AddUse(linstr,
                                    LIROperand::kVirtual,
                                    linstr->inputs[i]);

          linstr->inputs[i] = value;
        }
      }
    }
  }
}

} // namespace internal
} // namespace candor
