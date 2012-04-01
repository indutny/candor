#include "lir.h"
#include "lir-instructions-x64.h"
#include "lir-instructions-x64-inl.h"
#include "hir.h"
#include "hir-instructions.h"
#include "macroassembler.h" // Masm
#include "stubs.h" // Stubs

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
        __ movq(ToRegister(target_op), ToRegister(source_op));
      } else {
        __ movq(ToOperand(target_op), ToRegister(source_op));
      }
    } else {
      if (target_op->is_register()) {
        __ movq(ToRegister(target_op), ToOperand(source_op));
      } else {
        __ movq(scratch, ToOperand(source_op));
        __ movq(ToOperand(target_op), scratch);
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
    if (!ToRegister(inputs[0]).is(rax)) {
      __ movq(rax, ToRegister(inputs[0]));
    }
  } else if (inputs[0]->is_spill()) {
    __ movq(rax, ToOperand(inputs[0]));
  }

  __ movq(rsp, rbp);
  __ pop(rbp);
  __ ret(0);
}


void LIRGoto::Generate() {
  assert(hir()->block()->successors_count() == 1);

  // Zero jumps should be ignored
  if (hir()->next() == hir()->block()->successors()[0]->first_instruction()) {
    return;
  }

  // Generate jmp and add relocation info to block
  __ jmp(NULL);
  RelocationInfo* addr = new RelocationInfo(RelocationInfo::kRelative,
                                            RelocationInfo::kLong,
                                            masm()->offset() - 4);
  hir()->block()->successors()[0]->uses()->Push(addr);
}


void LIRStoreLocal::Generate() {
  // NOTE: Store acts in reverse order - input = result
  // that's needed for result propagation in chain assignments
  if (inputs[0]->is_register()) {
    if (result->is_register()) {
      __ movq(ToRegister(inputs[0]), ToRegister(result));
    } else if (result->is_immediate()) {
      __ movq(ToRegister(inputs[0]), Immediate(result->value()));
    }
  } else if (inputs[0]->is_spill()) {
    if (result->is_register()) {
      __ movq(ToOperand(inputs[0]), ToRegister(result));
    } else if (result->is_immediate()) {
      __ movq(ToOperand(inputs[0]), Immediate(result->value()));
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

  __ movq(ToRegister(result), root_slot);
}


void LIRLoadLocal::Generate() {
}


void LIRLoadContext::Generate() {
}


void LIRBranchBool::Generate() {
  // NOTE: input is definitely a register here
  if (!ToRegister(inputs[0]).is(rax)) {
    __ movq(rax, ToRegister(inputs[0]));
  }

  // Coerce value to boolean first
  __ Call(masm()->stubs()->GetCoerceToBooleanStub());

  // Jmp to `right` block if value is `false`
  Operand bvalue(rax, HBoolean::kValueOffset);
  __ cmpb(bvalue, Immediate(0));
  __ jmp(kEq, NULL);

  RelocationInfo* addr = new RelocationInfo(RelocationInfo::kRelative,
                                            RelocationInfo::kLong,
                                            masm()->offset() - 4);
  hir()->right()->successors()[0]->uses()->Push(addr);
}


void LIRCall::Generate() {
}


void LIRAllocateContext::Generate() {
}


void LIRAllocateFunction::Generate() {
  // Get function's body address by generating relocation info
  __ movq(ToRegister(scratches[0]), Immediate(0));
  RelocationInfo* addr = new RelocationInfo(RelocationInfo::kAbsolute,
                                            RelocationInfo::kQuad,
                                            masm()->offset() - 8);
  hir()->body()->uses()->Push(addr);

  // Call stub
  __ push(Immediate(hir()->argc()));
  __ push(ToRegister(scratches[0]));
  __ Call(masm()->stubs()->GetAllocateFunctionStub());

  // Propagate result
  if (!ToRegister(result).is(rax)) {
    __ movq(ToRegister(result), rax);
  }
}


void LIRAllocateObject::Generate() {
}

} // namespace internal
} // namespace candor
