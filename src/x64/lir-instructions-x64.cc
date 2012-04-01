#include "lir.h"
#include "lir-instructions-x64.h"
#include "hir.h"
#include "hir-instructions.h"
#include "macroassembler.h"

#include "scope.h" // ScopeSlot
#include "heap.h" // HContext::GetIndexDisp
#include "heap-inl.h"

#include <stdlib.h> // NULL
#include <assert.h> // assert

namespace candor {
namespace internal {

#define __ masm()->

void LIRParallelMove::Generate() {
  ZoneList<LIROperand*>::Item* source = hir()->sources()->head();
  ZoneList<LIROperand*>::Item* target = hir()->targets()->head();

  for (; source != NULL; source = source->next(), target = target->next()) {
    LIROperand* source_op = source->value();
    LIROperand* target_op = target->value();

    if (source_op->is_register()) {
      if (target_op->is_register()) {
        __ movq(RegisterByIndex(target_op->value()),
                RegisterByIndex(source_op->value()));
      } else {
        __ movq(masm()->SpillToOperand(target_op->value()),
                RegisterByIndex(source_op->value()));
      }
    } else {
      if (target_op->is_register()) {
        __ movq(RegisterByIndex(target_op->value()),
                masm()->SpillToOperand(source_op->value()));
      } else {
        __ movq(scratch, masm()->SpillToOperand(source_op->value()));
        __ movq(masm()->SpillToOperand(target_op->value()), scratch);
      }
    }
  }
}


void LIREntry::Generate() {
  __ push(rbp);
  __ movq(rbp, rsp);

  __ AllocateSpills();
}


void LIRReturn::Generate() {
  if (inputs[0]->is_immediate()) {
    __ movq(rax, Immediate(inputs[0]->value()));
  } else if (inputs[0]->is_register()) {
    if (!RegisterByIndex(inputs[0]->value()).is(rax)) {
      __ movq(rax, RegisterByIndex(inputs[0]->value()));
    }
  } else if (inputs[0]->is_spill()) {
    __ movq(rax, masm()->SpillToOperand(inputs[0]->value()));
  }

  __ movq(rsp, rbp);
  __ pop(rbp);
  __ ret(0);
}


void LIRGoto::Generate() {
}


void LIRStoreLocal::Generate() {
  if (inputs[0]->is_register()) {
    Register slot = RegisterByIndex(inputs[0]->value());
    if (result->is_register()) {
      __ movq(slot, RegisterByIndex(result->value()));
    } else if (result->is_immediate()) {
      __ movq(slot, Immediate(result->value()));
    }
  } else if (inputs[0]->is_spill()) {
    Operand slot = masm()->SpillToOperand(inputs[0]->value());
    if (result->is_register()) {
      __ movq(slot, RegisterByIndex(result->value()));
    } else if (result->is_immediate()) {
      __ movq(slot, Immediate(result->value()));
    }
  } else {
    UNEXPECTED
  }
}


void LIRStoreContext::Generate() {
}


void LIRStoreProperty::Generate() {
}


void LIRLoadRoot::Generate() {
  // root()->Place(...) may generate immediate values
  // ignore them here
  ScopeSlot* slot = hir()->value()->slot();
  if (slot->is_immediate()) return;
  assert(slot->is_context());

  Operand root_slot(root_reg, HContext::GetIndexDisp(slot->index()));

  __ movq(RegisterByIndex(result->value()), root_slot);
}


void LIRLoadLocal::Generate() {
}


void LIRLoadContext::Generate() {
}


void LIRBranchBool::Generate() {
}


void LIRAllocateContext::Generate() {
}


void LIRAllocateFunction::Generate() {
}


void LIRAllocateObject::Generate() {
}

} // namespace internal
} // namespace candor
