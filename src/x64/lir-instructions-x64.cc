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
    // Fast-case for : reg = 0
    if (target->value()->is_register() &&
        source->value()->is_immediate() && source->value()->value() == 0) {
      __ xorq(ToRegister(target->value()), ToRegister(target->value()));
      continue;
    }
    __ Mov(target->value(), source->value());
  }
}


void LIRNop::Generate() {
}


void LIREntry::Generate() {
  __ push(rbp);
  __ mov(rbp, rsp);

  __ AllocateSpills();
  __ FillStackSlots();

  // Allocate context
  __ AllocateContext(hir()->context_slots());

  // If function has arguments
  HIRValueList::Item* arg = hir()->args()->head();
  if (arg != NULL) {
    // Put argc into root register (temporarily)
    __ push(root_reg);
    __ mov(root_reg, rax);

    // Put nil in all args
    __ mov(scratch, Immediate(Heap::kTagNil));
    for (int i = 0; arg != NULL; arg = arg->next(), i++) {
      __ Mov(arg->value()->operand(), scratch);
    }

    // Move args to registers/spills
    Label args_end(masm());

    arg = hir()->args()->head();
    for (int i = 0; arg != NULL; arg = arg->next(), i++) {
      __ cmpq(root_reg, Immediate(HNumber::Tag(i + 1)));
      __ jmp(kLt, &args_end);

      // Skip return address and previous rbp
      Operand arg_slot(rbp, (i + 2) * 8);
      __ Mov(arg->value()->operand(), arg_slot);
    }
    __ pop(root_reg);

    __ bind(&args_end);
  }
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
  // NOTE: Next instruction is a move instruction (skip it)
  if (hir()->next()->next() ==
      hir()->block()->successors()[0]->first_instruction()) {
    return;
  }

  // Generate jmp and add relocation info to block
  __ jmp(NULL);
  RelocationInfo* addr = new RelocationInfo(RelocationInfo::kRelative,
                                            RelocationInfo::kLong,
                                            masm()->offset() - 4);
  hir()->block()->successors()[0]->AddUse(addr);
}


void LIRStoreLocal::Generate() {
  // NOTE: Store acts in reverse order - input = result
  // that's needed for result propagation in chain assignments
  __ Mov(result, inputs[0]);
}


void LIRStoreContext::Generate() {
  ScopeSlot* slot = hir()->lhs()->slot();
  int depth = slot->depth();

  if (depth == -1) {
    return;
  }

  __ Mov(scratches[0], context_reg);

  // Lookup context
  while (--depth >= 0) {
    Operand parent(ToRegister(scratches[0]), HContext::kParentOffset);
    __ Mov(scratches[0], parent);
  }

  Operand res(ToRegister(scratches[0]),
              HContext::GetIndexDisp(slot->index()));
  __ Mov(res, inputs[0]);
}


LIRStoreProperty::LIRStoreProperty() {
  inputs[0] = ToLIROperand(rax);
  inputs[1] = ToLIROperand(rbx);
}


void LIRStoreProperty::Generate() {
  __ Push(inputs[2]);
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

  // Result may be rax
  __ Mov(scratches[0], rax);

  __ pop(rbx);
  __ Pop(inputs[2]);

  __ IsNil(ToRegister(scratches[0]), NULL, &done);
  Operand qmap(rbx, HObject::kMapOffset);
  __ mov(rbx, qmap);
  __ addq(rax, rbx);

  Operand slot(rax, 0);
  __ Mov(scratch, inputs[2]);
  __ mov(slot, scratch);

  __ bind(&done);
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


void LIRLoadContext::Generate() {
  ScopeSlot* slot = hir()->value()->slot();
  int depth = slot->depth();

  if (depth == -1) {
    // Global object lookup
    Operand global(root_reg, HContext::GetIndexDisp(Heap::kRootGlobalIndex));
    __ Mov(result, global);
    return;
  }

  __ Mov(result, context_reg);

  // Lookup context
  while (--depth >= 0) {
    Operand parent(ToRegister(result), HContext::kParentOffset);
    __ Mov(result, parent);
  }

  Operand res(ToRegister(result),
              HContext::GetIndexDisp(slot->index()));
  __ Mov(result, res);
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


LIRBranchBool::LIRBranchBool() {
  inputs[0] = ToLIROperand(rax);
}


void LIRBranchBool::Generate() {
  // Coerce value to boolean first
  __ Call(masm()->stubs()->GetCoerceToBooleanStub());

  // Jmp to `right` block if value is `false`
  Operand bvalue(rax, HBoolean::kValueOffset);
  __ cmpb(bvalue, Immediate(0));

  // Generate reverse-movement before jumping
  lir()->GenerateReverseMove(masm(), hir());

  // Jump to the far block
  if (hir()->next() == hir()->block()->successors()[0]->first_instruction()) {
    __ jmp(kNe, NULL);

    RelocationInfo* addr = new RelocationInfo(RelocationInfo::kRelative,
                                              RelocationInfo::kLong,
                                              masm()->offset() - 4);
    hir()->left()->AddUse(addr);
  } else {
    __ jmp(kEq, NULL);

    RelocationInfo* addr = new RelocationInfo(RelocationInfo::kRelative,
                                              RelocationInfo::kLong,
                                              masm()->offset() - 4);
    hir()->right()->AddUse(addr);
  }
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
  Label not_function(masm()), done(masm());

  __ Mov(scratch, inputs[0]);
  __ IsUnboxed(scratch, NULL, &not_function);
  __ IsNil(scratch, NULL, &not_function);
  __ IsHeapObject(Heap::kTagFunction, scratch, &not_function, NULL);

  Masm::Spill ctx(masm(), context_reg), root(masm(), root_reg);
  Masm::Spill rsp_s(masm(), rsp);

  __ mov(scratch, Immediate(HNumber::Tag(hir()->args()->length())));
  Masm::Spill argc_s(masm(), scratch);

  // If argc is odd - align stack
  Label even(masm());
  __ testb(scratch, Immediate(HNumber::Tag(1)));
  __ jmp(kEq, &even);
  __ push(Immediate(Heap::kTagNil));
  __ bind(&even);

  HIRValueList::Item* arg = hir()->args()->tail();
  for (; arg != NULL; arg = arg->prev()) {
    __ Push(arg->value()->operand());
  }

  __ Mov(scratch, inputs[0]);
  argc_s.Unspill(rax);

  __ CallFunction(scratch);

  // Reset all registers to nil
  __ mov(scratch, Immediate(Heap::kTagNil));
  __ mov(rbx, scratch);
  __ mov(rcx, scratch);
  __ mov(rdx, scratch);
  __ mov(r8, scratch);
  __ mov(r9, scratch);
  __ mov(r10, scratch);
  __ mov(r11, scratch);
  __ mov(r12, scratch);
  __ mov(r13, scratch);

  // Store result
  __ Mov(result, rax);

  rsp_s.Unspill();
  root.Unspill();
  ctx.Unspill();

  __ jmp(&done);
  __ bind(&not_function);

  __ Mov(result, Immediate(Heap::kTagNil));

  __ bind(&done);
}


LIRTypeof::LIRTypeof() {
  inputs[0] = ToLIROperand(rax);
}


LIRSizeof::LIRSizeof() {
  inputs[0] = ToLIROperand(rax);
}


LIRKeysof::LIRKeysof() {
  inputs[0] = ToLIROperand(rax);
}


void LIRTypeof::Generate() {
  __ Call(masm()->stubs()->GetTypeofStub());
  __ Mov(result, rax);
}


void LIRSizeof::Generate() {
  __ Call(masm()->stubs()->GetSizeofStub());
  __ Mov(result, rax);
}


void LIRKeysof::Generate() {
  __ Call(masm()->stubs()->GetKeysofStub());
  __ Mov(result, rax);
}


LIRNot::LIRNot() {
  inputs[0] = ToLIROperand(rax);
}


void LIRNot::Generate() {
  // Coerce value to boolean first
  __ Call(masm()->stubs()->GetCoerceToBooleanStub());

  Label on_false(masm()), done(masm());

  // Jmp to `right` block if value is `false`
  Operand bvalue(rax, HBoolean::kValueOffset);
  __ cmpb(bvalue, Immediate(0));
  __ jmp(kEq, &on_false);

  Operand truev(root_reg, HContext::GetIndexDisp(Heap::kRootTrueIndex));
  Operand falsev(root_reg, HContext::GetIndexDisp(Heap::kRootFalseIndex));

  // !true = false
  __ Mov(result, falsev);

  __ jmp(&done);
  __ bind(&on_false);

  // !false = true
  __ Mov(result, truev);

  __ bind(&done);
}


void LIRCollectGarbage::Generate() {
  __ Call(masm()->stubs()->GetCollectGarbageStub());
  __ Mov(result, Immediate(Heap::kTagNil));
}


void LIRGetStackTrace::Generate() {
  uint32_t ip = masm()->offset();

  // Pass ip
  __ mov(rax, Immediate(0));
  RelocationInfo* r = new RelocationInfo(RelocationInfo::kAbsolute,
                                         RelocationInfo::kQuad,
                                         masm()->offset() - 8);
  masm()->relocation_info_.Push(r);
  r->target(ip);
  __ Call(masm()->stubs()->GetStackTraceStub());

  __ Mov(result, Immediate(Heap::kTagNil));
}


void LIRAllocateFunction::Generate() {
  // Get function's body address by generating relocation info
  __ mov(ToRegister(scratches[0]), Immediate(0));
  RelocationInfo* addr = new RelocationInfo(RelocationInfo::kAbsolute,
                                            RelocationInfo::kQuad,
                                            masm()->offset() - 8);
  hir()->body()->AddUse(addr);

  // Call stub
  __ push(Immediate(hir()->argc()));
  __ push(ToRegister(scratches[0]));
  __ Call(masm()->stubs()->GetAllocateFunctionStub());

  // Propagate result
  __ Mov(result, rax);
}


void LIRAllocateObject::Generate() {
  __ push(Immediate(HNumber::Tag(hir()->size())));
  switch (hir()->kind()) {
   case HIRAllocateObject::kObject:
    __ push(Immediate(HNumber::Tag(Heap::kTagObject)));
    break;
   case HIRAllocateObject::kArray:
    __ push(Immediate(HNumber::Tag(Heap::kTagArray)));
    break;
   default:
    UNEXPECTED
    break;
  }
  __ Call(masm()->stubs()->GetAllocateObjectStub());

  __ Mov(result, rax);
}

} // namespace internal
} // namespace candor
