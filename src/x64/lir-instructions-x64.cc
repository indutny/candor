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
    __ Mov(target->value(), source->value());
  }
}


void LIRNop::Generate() {
  __ nop();
}


void LIREntry::Generate() {
  __ push(rbp);
  __ movq(rbp, rsp);

  __ AllocateSpills();
}


void LIRReturn::Generate() {
  __ Mov(rax, inputs[0]);

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
  __ Mov(inputs[0], result);
}


void LIRStoreContext::Generate() {
}


void LIRStoreProperty::Generate() {
  __ Push(result);
  __ Push(inputs[0]);

  __ Mov(rax, inputs[0]);
  __ Mov(rbx, inputs[1]);
  __ movq(rcx, Immediate(1));
  __ Call(masm()->stubs()->GetLookupPropertyStub());

  // Make rax look like unboxed number to GC
  __ dec(rax);
  __ CheckGC();
  __ inc(rax);

  Label done(masm());

  __ pop(rbx);
  __ pop(rcx);

  __ IsNil(rax, NULL, &done);
  Operand qmap(rbx, HObject::kMapOffset);
  __ movq(rbx, qmap);
  __ addq(rax, rbx);

  Operand slot(rax, 0);
  __ movq(slot, rcx);

  __ bind(&done);
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
  // Coerce value to boolean first
  __ Mov(rax, inputs[0]);
  __ Call(masm()->stubs()->GetCoerceToBooleanStub());

  // Jmp to `right` block if value is `false`
  Operand bvalue(rax, HBoolean::kValueOffset);
  __ cmpb(bvalue, Immediate(0));
  __ jmp(kEq, NULL);

  RelocationInfo* addr = new RelocationInfo(RelocationInfo::kRelative,
                                            RelocationInfo::kLong,
                                            masm()->offset() - 4);
  hir()->right()->uses()->Push(addr);
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
