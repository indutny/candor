#include "lir.h"
#include "lir-instructions-x64.h"
#include "lir-instructions-x64-inl.h"
#include "ast.h" // BinOp
#include "hir.h"
#include "hir-instructions.h"
#include "macroassembler.h" // Masm
#include "macroassembler-inl.h"
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
  __ mov(rbp, rsp);

  __ AllocateSpills();

  // Put nil in all args
  HIRValueList::Item* arg = hir()->args()->head();
  if (arg != NULL) __ mov(scratch, Immediate(Heap::kTagNil));
  for (int i = 0; arg != NULL; arg = arg->next(), i++) {
    __ Mov(arg->value()->operand(), scratch);
  }

  // Move args to registers/spills
  Label args_end(masm());

  arg = hir()->args()->head();
  for (int i = 0; arg != NULL; arg = arg->next(), i++) {
    __ cmpq(rsi, Immediate(HNumber::Tag(i + 1)));
    __ jmp(kLt, &args_end);

    // Skip return address and previous rbp
    Operand arg_slot(rbp, (i + 2) * 8);
    __ Mov(arg->value()->operand(), arg_slot);
  }

  __ bind(&args_end);
}


void LIRReturn::Generate() {
  __ Mov(rax, inputs[0]);

  __ mov(rsp, rbp);
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


LIRStoreProperty::LIRStoreProperty() {
  inputs[0] = ToLIROperand(rax);
  inputs[1] = ToLIROperand(rbx);
}


void LIRStoreProperty::Generate() {
  __ Push(result);
  __ Push(inputs[0]);

  // rax <- object
  // rbx <- property
  __ mov(rcx, Immediate(1));
  __ Call(masm()->stubs()->GetLookupPropertyStub());

  // Make rax look like unboxed number to GC
  __ dec(rax);
  __ CheckGC();
  __ inc(rax);

  Label done(masm());

  __ pop(rbx);

  __ IsNil(rax, NULL, &done);
  Operand qmap(rbx, HObject::kMapOffset);
  __ mov(rbx, qmap);
  __ addq(rax, rbx);

  Operand slot(rax, 0);
  __ Mov(scratch, result);
  __ mov(slot, scratch);

  __ bind(&done);
  __ Pop(result);
}


void LIRLoadRoot::Generate() {
  // root()->Place(...) may generate immediate values
  // ignore them here
  ScopeSlot* slot = hir()->value()->slot();
  if (slot->is_immediate()) return;
  assert(slot->is_context());

  Operand root_slot(root_reg, HContext::GetIndexDisp(slot->index()));

  __ mov(ToRegister(result), root_slot);
}


void LIRLoadLocal::Generate() {
}


void LIRLoadContext::Generate() {
}


LIRLoadProperty::LIRLoadProperty() {
  inputs[0] = ToLIROperand(rax);
  inputs[1] = ToLIROperand(rbx);
}


void LIRLoadProperty::Generate() {
  __ push(rax);
  __ push(rax);

  // rax <- object
  // rbx <- property
  __ mov(rcx, Immediate(0));
  __ Call(masm()->stubs()->GetLookupPropertyStub());

  Label done(masm());

  __ pop(rbx);
  __ pop(rbx);

  __ IsNil(rax, NULL, &done);
  Operand qmap(rbx, HObject::kMapOffset);
  __ mov(rbx, qmap);
  __ addq(rax, rbx);

  Operand slot(rax, 0);
  __ mov(rax, slot);

  __ bind(&done);
  __ Mov(result, rax);
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


LIRBinOp::LIRBinOp() {
  inputs[0] = ToLIROperand(rax);
  inputs[1] = ToLIROperand(rbx);
}


void LIRBinOp::Generate() {
  Label call_stub(masm()), done(masm());

  // Fast case : unboxed +/- const
  if (inputs[1]->is_immediate() &&
      (hir()->type() == BinOp::kAdd || hir()->type() == BinOp::kSub)) {
    __ IsUnboxed(rax, &call_stub, NULL);

    int64_t num = inputs[1]->value();

    // addq and subq supports only long immediate (not quad)
    if (num >= -0x7fffffff && num <= 0x7fffffff) {
      switch (hir()->type()) {
       case BinOp::kAdd: __ addq(rax, Immediate(num)); break;
       case BinOp::kSub: __ subq(rax, Immediate(num)); break;
       default:
        UNEXPECTED
        break;
      }

      __ jmp(kNoOverflow, &done);

      // Restore on overflow
      switch (hir()->type()) {
       case BinOp::kAdd: __ subq(rax, Immediate(num)); break;
       case BinOp::kSub: __ addq(rax, Immediate(num)); break;
       default:
        UNEXPECTED
        break;
      }
    }
  }

  char* stub = NULL;

#define BINARY_SUB_TYPES(V)\
    V(Add)\
    V(Sub)\
    V(Mul)\
    V(Div)\
    V(Mod)\
    V(BAnd)\
    V(BOr)\
    V(BXor)\
    V(Shl)\
    V(Shr)\
    V(UShr)\
    V(Eq)\
    V(StrictEq)\
    V(Ne)\
    V(StrictNe)\
    V(Lt)\
    V(Gt)\
    V(Le)\
    V(Ge)\
    V(LOr)\
    V(LAnd)

#define BINARY_SUB_ENUM(V)\
    case BinOp::k##V: stub = masm()->stubs()->GetBinary##V##Stub(); break;


  switch (hir()->type()) {
   BINARY_SUB_TYPES(BINARY_SUB_ENUM)
   default: __ emitb(0xcc); break;
  }
#undef BINARY_SUB_ENUM
#undef BINARY_SUB_TYPES

  assert(stub != NULL);

  __ bind(&call_stub);

  __ Call(stub);

  __ bind(&done);
  __ Mov(result, rax);
}


void LIRCall::Generate() {
  Masm::Spill rsi_s(masm(), rsi), rdi_s(masm(), rdi), rsp_s(masm(), rsp);

  __ mov(rsi, Immediate(HNumber::Tag(hir()->args()->length())));

  // If argc is odd - align stack
  Label even(masm());
  __ testb(rsi, Immediate(1));
  __ jmp(kEq, &even);
  __ push(Immediate(Heap::kTagNil));
  __ bind(&even);

  HIRValueList::Item* arg = hir()->args()->tail();
  for (; arg != NULL; arg = arg->prev()) {
    __ Push(arg->value()->operand());
  }

  __ Mov(scratch, inputs[0]);
  __ CallFunction(scratch);
  __ Mov(result, rax);

  rsp_s.Unspill();
  rdi_s.Unspill();
  rsi_s.Unspill();
}


void LIRAllocateContext::Generate() {
}


void LIRAllocateFunction::Generate() {
  // Get function's body address by generating relocation info
  __ mov(ToRegister(scratches[0]), Immediate(0));
  RelocationInfo* addr = new RelocationInfo(RelocationInfo::kAbsolute,
                                            RelocationInfo::kQuad,
                                            masm()->offset() - 8);
  hir()->body()->uses()->Push(addr);

  // Call stub
  __ push(Immediate(hir()->argc()));
  __ push(ToRegister(scratches[0]));
  __ Call(masm()->stubs()->GetAllocateFunctionStub());

  // Propagate result
  __ Mov(result, rax);
}


void LIRAllocateObject::Generate() {
  __ push(Immediate(hir()->size()));
  __ push(Immediate(hir()->kind()));
  __ Call(masm()->stubs()->GetAllocateObjectStub());

  __ Mov(result, rax);
}

} // namespace internal
} // namespace candor
