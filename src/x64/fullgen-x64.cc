/**
 * Copyright (c) 2012, Fedor Indutny.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "fullgen.h"
#include "fullgen-inl.h"
#include "fullgen-instructions.h"
#include "fullgen-instructions-inl.h"
#include "heap.h"
#include "heap-inl.h"
#include "macroassembler.h"
#include "stubs.h"

namespace candor {
namespace internal {

#define __ masm->

Operand* FOperand::ToOperand() {
  assert(type_ == kStack);

  // Argc and return address
  return new Operand(rbp, -HValue::kPointerSize * (index_ + 3));
}


void FNop::Generate(Masm* masm) {
  // Nop
}


void FNil::Generate(Masm* masm) {
  __ mov(*result->ToOperand(), Immediate(Heap::kTagNil));
}


void FLabel::Generate(Masm* masm) {
  __ bind(&label);
}


void FEntry::Generate(Masm* masm) {
  __ push(rbp);
  __ mov(rbp, rsp);

  // Allocate spills
  __ AllocateSpills();

  // Save argc
  Operand argc(rbp, -HValue::kPointerSize * 2);
  __ mov(argc, rax);

  // Allocate context slots
  __ AllocateContext(context_slots_);
}


void FReturn::Generate(Masm* masm) {
  __ mov(rax, *inputs[0]->ToOperand());
  __ mov(rsp, rbp);
  __ pop(rbp);
  __ ret(0);
}


void FChi::Generate(Masm* masm) {
  // Just move input to output
  __ mov(scratch, *inputs[0]->ToOperand());
  __ mov(*result->ToOperand(), scratch);
}


void FLiteral::Generate(Masm* masm) {
  if (slot_->is_immediate()) {
    __ mov(*result->ToOperand(),
           Immediate(reinterpret_cast<intptr_t>(slot_->value())));
  } else {
    assert(slot_->is_context());
    assert(slot_->depth() == -2);
    Operand slot(root_reg, HContext::GetIndexDisp(slot_->index()));
    __ mov(scratch, slot);
    __ mov(*result->ToOperand(), scratch);
  }
}


void FStore::Generate(Masm* masm) {
  if (inputs[1] == inputs[0]) return;
  __ mov(scratch, *inputs[1]->ToOperand());
  __ mov(*inputs[0]->ToOperand(), scratch);
}


void FLoad::Generate(Masm* masm) {
  if (result == inputs[0]) return;
  __ mov(scratch, *inputs[0]->ToOperand());
  __ mov(*result->ToOperand(), scratch);
}


void FStoreContext::Generate(Masm* masm) {
  assert(inputs[0]->is_context());

  int depth = inputs[0]->depth();

  // Global can't be replaced
  if (depth == -1) return;

  __ mov(scratch, context_reg);

  // Lookup context
  while (--depth >= 0) {
    Operand parent(scratch, HContext::kParentOffset);
    __ mov(scratch, parent);
  }

  Operand res(scratch, HContext::GetIndexDisp(inputs[0]->index()));
  __ mov(rax, *inputs[1]->ToOperand());
  __ mov(res, rax);
}


void FLoadContext::Generate(Masm* masm) {
  assert(inputs[0]->is_context());

  int depth = inputs[0]->depth();

  if (depth == -1) {
    // Global object lookup
    Operand global(root_reg, HContext::GetIndexDisp(Heap::kRootGlobalIndex));
    __ mov(scratch, global);
    __ mov(*result->ToOperand(), scratch);
    return;
  }

  __ mov(scratch, context_reg);

  // Lookup context
  while (--depth >= 0) {
    Operand parent(scratch, HContext::kParentOffset);
    __ mov(scratch, parent);
  }

  Operand res(scratch, HContext::GetIndexDisp(inputs[0]->index()));
  __ mov(scratch, res);
  __ mov(*result->ToOperand(), scratch);
}


void FStoreProperty::Generate(Masm* masm) {
  Label done;
  __ mov(rax, *inputs[0]->ToOperand());
  __ mov(rbx, *inputs[1]->ToOperand());

  // rax <- object
  // rbx <- propery
  // rcx <- value
  __ mov(rcx, Immediate(1));
  __ Call(masm->stubs()->GetLookupPropertyStub());

  // Make rax look like unboxed number to GC
  __ dec(rax);
  __ CheckGC();
  __ inc(rax);

  __ IsNil(rax, NULL, &done);
  __ mov(rbx, *inputs[0]->ToOperand());
  __ mov(rcx, *inputs[2]->ToOperand());
  Operand qmap(rbx, HObject::kMapOffset);
  __ mov(rbx, qmap);
  __ addq(rax, rbx);

  Operand slot(rax, 0);
  __ mov(slot, rcx);

  __ bind(&done);
}


void FLoadProperty::Generate(Masm* masm) {
  Label done;
  __ mov(rax, *inputs[0]->ToOperand());
  __ mov(rbx, *inputs[1]->ToOperand());

  // rax <- object
  // rbx <- propery
  __ mov(rcx, Immediate(0));
  __ Call(masm->stubs()->GetLookupPropertyStub());

  __ IsNil(rax, NULL, &done);
  __ mov(rbx, *inputs[0]->ToOperand());
  Operand qmap(rbx, HObject::kMapOffset);
  __ mov(rbx, qmap);
  __ addq(rax, rbx);

  Operand slot(rax, 0);
  __ mov(rax, slot);

  __ bind(&done);
  __ mov(*result->ToOperand(), rax);
}


void FDeleteProperty::Generate(Masm* masm) {
  // rax <- object
  // rbx <- property
  __ mov(rax, *inputs[0]->ToOperand());
  __ mov(rbx, *inputs[1]->ToOperand());
  __ Call(masm->stubs()->GetDeletePropertyStub());
}


void FAllocateObject::Generate(Masm* masm) {
  __ push(Immediate(HNumber::Tag(size_)));
  __ pushb(Immediate(HNumber::Tag(Heap::kTagObject)));
  __ Call(masm->stubs()->GetAllocateObjectStub());
  __ mov(*result->ToOperand(), rax);
}


void FAllocateArray::Generate(Masm* masm) {
  __ push(Immediate(HNumber::Tag(size_)));
  __ pushb(Immediate(HNumber::Tag(Heap::kTagArray)));
  __ Call(masm->stubs()->GetAllocateObjectStub());
  __ mov(*result->ToOperand(), rax);
}


void FIf::Generate(Masm* masm) {
  __ mov(rax, *inputs[0]->ToOperand());
  __ Call(masm->stubs()->GetCoerceToBooleanStub());

  // Jmp to `right` block if value is `false`
  Operand bvalue(rax, HBoolean::kValueOffset);
  __ cmpb(bvalue, Immediate(0));
  __ jmp(kEq, &f_->label);
  __ jmp(kNe, &t_->label);
}


void FGoto::Generate(Masm* masm) {
  __ jmp(&label_->label);
}


void FBreak::Generate(Masm* masm) {
  __ jmp(&label_->label);
}


void FContinue::Generate(Masm* masm) {
  __ jmp(&label_->label);
}


void FNot::Generate(Masm* masm) {
  // rax <- value
  __ mov(rax, *inputs[0]->ToOperand());

  // Coerce value to boolean first
  __ Call(masm->stubs()->GetCoerceToBooleanStub());

  Label on_false, done;

  // Jmp to `right` block if value is `false`
  Operand bvalue(rax, HBoolean::kValueOffset);
  __ cmpb(bvalue, Immediate(0));
  __ jmp(kEq, &on_false);

  Operand truev(root_reg, HContext::GetIndexDisp(Heap::kRootTrueIndex));
  Operand falsev(root_reg, HContext::GetIndexDisp(Heap::kRootFalseIndex));

  // !true = false
  __ mov(rax, falsev);

  __ jmp(&done);
  __ bind(&on_false);

  // !false = true
  __ mov(rax, truev);

  __ bind(&done);

  // result -> rax
  __ mov(*result->ToOperand(), rax);
}

#define BINARY_SUB_TYPES(V) \
    V(Add) \
    V(Sub) \
    V(Mul) \
    V(Div) \
    V(Mod) \
    V(BAnd) \
    V(BOr) \
    V(BXor) \
    V(Shl) \
    V(Shr) \
    V(UShr) \
    V(Eq) \
    V(StrictEq) \
    V(Ne) \
    V(StrictNe) \
    V(Lt) \
    V(Gt) \
    V(Le) \
    V(Ge)

#define BINARY_SUB_ENUM(V)\
    case BinOp::k##V: stub = masm->stubs()->GetBinary##V##Stub(); break;

void FBinOp::Generate(Masm* masm) {
  char* stub = NULL;

  switch (sub_type_) {
    BINARY_SUB_TYPES(BINARY_SUB_ENUM)
    default: UNEXPECTED
  }

  assert(stub != NULL);

  // rax <- lhs
  // rbx <- rhs
  __ mov(rax, *inputs[0]->ToOperand());
  __ mov(rbx, *inputs[1]->ToOperand());
  __ Call(stub);
  // result -> rax
  __ mov(*result->ToOperand(), rax);
}

#undef BINARY_SUB_TYPES
#undef BINARY_SUB_ENUM

void FFunction::Generate(Masm* masm) {
  // Get function's body address from relocation info
  __ mov(scratch, Immediate(0));
  RelocationInfo* addr = new RelocationInfo(RelocationInfo::kAbsolute,
                                            RelocationInfo::kQuad,
                                            masm->offset() - 8);
  body->label.AddUse(masm, addr);

  // Call stub
  __ push(Immediate(HNumber::Tag(argc_)));
  __ push(scratch);
  __ Call(masm->stubs()->GetAllocateFunctionStub());
  __ mov(*result->ToOperand(), rax);
}


void FClone::Generate(Masm* masm) {
  __ mov(rax, *inputs[0]->ToOperand());
  __ Call(masm->stubs()->GetCloneObjectStub());
  __ mov(*result->ToOperand(), rax);
}


void FSizeof::Generate(Masm* masm) {
  __ mov(rax, *inputs[0]->ToOperand());
  __ Call(masm->stubs()->GetSizeofStub());
  __ mov(*result->ToOperand(), rax);
}


void FKeysof::Generate(Masm* masm) {
  __ mov(rax, *inputs[0]->ToOperand());
  __ Call(masm->stubs()->GetKeysofStub());
  __ mov(*result->ToOperand(), rax);
}


void FTypeof::Generate(Masm* masm) {
  __ mov(rax, *inputs[0]->ToOperand());
  __ Call(masm->stubs()->GetTypeofStub());
  __ mov(*result->ToOperand(), rax);
}


void FStoreArg::Generate(Masm* masm) {
  // Calculate slot position
  __ mov(rax, rsp);
  __ mov(rbx, *inputs[1]->ToOperand());
  __ shl(rbx, Immediate(2));
  __ addq(rax, rbx);
  Operand slot(rax, 0);
  __ mov(scratch, *inputs[0]->ToOperand());
  __ mov(slot, scratch);
}


void FStoreVarArg::Generate(Masm* masm) {
  __ mov(rax, *inputs[0]->ToOperand());
  __ mov(rdx, rsp);
  __ shl(rax, Immediate(2));
  __ addq(rdx, *inputs[1]->ToOperand());
  __ Call(masm->stubs()->GetStoreVarArgStub());
}


void FAlignStack::Generate(Masm* masm) {
  Label even;
  __ mov(scratch, *inputs[0]->ToOperand());
  __ testb(scratch, Immediate(HNumber::Tag(1)));
  __ jmp(kEq, &even);
  __ pushb(Immediate(Heap::kTagNil));
  __ bind(&even);

  // Now allocate space on-stack for arguments
  __ mov(rax, *inputs[0]->ToOperand());
  __ shl(rax, Immediate(2));
  __ subq(rsp, rax);
}


void FLoadArg::Generate(Masm* masm) {
  Operand slot(scratch, 0);

  Label oob, skip;

  // NOTE: input is aligned number
  __ mov(scratch, *inputs[0]->ToOperand());

  // Check if we're trying to get argument that wasn't passed in
  Operand argc(rbp, -HValue::kPointerSize * 2);
  __ cmpq(scratch, argc);
  __ jmp(kGe, &oob);

  __ addqb(scratch, Immediate(HNumber::Tag(2)));
  __ shl(scratch, Immediate(2));
  __ addq(scratch, rbp);
  __ mov(rax, slot);
  __ mov(*result->ToOperand(), rax);

  __ jmp(&skip);
  __ bind(&oob);

  __ mov(*result->ToOperand(), Immediate(Heap::kTagNil));

  __ bind(&skip);
}


void FLoadVarArg::Generate(Masm* masm) {
  __ mov(rax, *inputs[0]->ToOperand());
  __ mov(rbx, *inputs[1]->ToOperand());
  __ mov(rcx, *inputs[2]->ToOperand());
  __ Call(masm->stubs()->GetLoadVarArgStub());
}


void FCall::Generate(Masm* masm) {
  Label not_function, even_argc, done;

  __ mov(rbx, *inputs[0]->ToOperand());
  __ mov(rax, *inputs[1]->ToOperand());

  // argc * 2
  __ mov(scratch, rax);

  __ testb(scratch, Immediate(HNumber::Tag(1)));
  __ jmp(kEq, &even_argc);
  __ addqb(scratch, Immediate(HNumber::Tag(1)));
  __ bind(&even_argc);
  __ shl(scratch, Immediate(2));
  __ addq(scratch, rsp);
  Masm::Spill rsp_s(masm, scratch);

  // rax <- argc
  // rbx <- fn

  __ IsUnboxed(rbx, NULL, &not_function);
  __ IsNil(rbx, NULL, &not_function);
  __ IsHeapObject(Heap::kTagFunction, rbx, &not_function, NULL);

  Masm::Spill ctx(masm, context_reg), root(masm, root_reg);
  Masm::Spill fn_s(masm, rbx);

  // rax <- argc
  // scratch <- fn
  __ mov(scratch, rbx);
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

  fn_s.Unspill();
  root.Unspill();
  ctx.Unspill();

  __ jmp(&done);
  __ bind(&not_function);

  __ mov(rax, Immediate(Heap::kTagNil));

  __ bind(&done);
  __ mov(*result->ToOperand(), rax);

  // Unwind all arguments pushed on stack
  rsp_s.Unspill(rsp);
}


void FCollectGarbage::Generate(Masm* masm) {
  __ Call(masm->stubs()->GetCollectGarbageStub());
}


void FGetStackTrace::Generate(Masm* masm) {
  AbsoluteAddress addr;

  addr.Target(masm, masm->offset());

  // Pass ip
  __ mov(rax, Immediate(0));
  addr.Use(masm, masm->offset() - 8);
  __ Call(masm->stubs()->GetStackTraceStub());

  __ mov(*result->ToOperand(), rax);
}

}  // namespace internal
}  // namespace candor
