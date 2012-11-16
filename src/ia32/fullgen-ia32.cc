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
  return new Operand(ebp, -HValue::kPointerSize * (index_ + 3));
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
  __ push(ebp);
  __ mov(ebp, esp);

  // Allocate spills
  __ AllocateSpills();

  // Save argc
  Operand argc(ebp, -HValue::kPointerSize * 2);
  __ mov(argc, eax);

  // Allocate context slots
  __ AllocateContext(context_slots_);
}


void FReturn::Generate(Masm* masm) {
  __ mov(eax, *inputs[0]->ToOperand());
  __ mov(esp, ebp);
  __ pop(ebp);
  __ ret(0);
}


void FChi::Generate(Masm* masm) {
  // Just move input to output
  __ mov(scratch, *inputs[0]->ToOperand());
  __ mov(*result->ToOperand(), scratch);
}


void FLiteral::Generate(Masm* masm) {
  Heap* heap = masm->heap();
  Immediate root(reinterpret_cast<intptr_t>(heap->old_space()->root()));
  Operand scratch_op(scratch, 0);

  if (slot_->is_immediate()) {
    __ mov(*result->ToOperand(),
           Immediate(reinterpret_cast<intptr_t>(slot_->value())));
  } else {
    assert(slot_->is_context());
    assert(slot_->depth() == -2);
    __ mov(scratch, root);
    __ mov(scratch, scratch_op);
    Operand slot(scratch, HContext::GetIndexDisp(slot_->index()));
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
  __ mov(eax, *inputs[1]->ToOperand());
  __ mov(res, eax);
}


void FLoadContext::Generate(Masm* masm) {
  assert(inputs[0]->is_context());

  Heap* heap = masm->heap();
  Immediate root(reinterpret_cast<intptr_t>(heap->old_space()->root()));
  int depth = inputs[0]->depth();

  if (depth == -1) {
    // Global object lookup
    Operand global(scratch, HContext::GetIndexDisp(Heap::kRootGlobalIndex));
    Operand scratch_op(scratch, 0);
    __ mov(scratch, root);
    __ mov(scratch, scratch_op);
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
  __ mov(eax, *inputs[0]->ToOperand());
  __ mov(ebx, *inputs[1]->ToOperand());

  // eax <- object
  // ebx <- propery
  // ecx <- value
  __ mov(ecx, Immediate(1));
  __ Call(masm->stubs()->GetLookupPropertyStub());

  // Make eax look like unboxed number to GC
  __ dec(eax);
  __ CheckGC();
  __ inc(eax);

  __ IsNil(eax, NULL, &done);
  __ mov(ebx, *inputs[0]->ToOperand());
  __ mov(ecx, *inputs[2]->ToOperand());
  Operand qmap(ebx, HObject::kMapOffset);
  __ mov(ebx, qmap);
  __ addl(eax, ebx);

  Operand slot(eax, 0);
  __ mov(slot, ecx);

  __ bind(&done);
}


void FLoadProperty::Generate(Masm* masm) {
  Label done;
  __ mov(eax, *inputs[0]->ToOperand());
  __ mov(ebx, *inputs[1]->ToOperand());

  // eax <- object
  // ebx <- propery
  __ mov(ecx, Immediate(0));
  __ Call(masm->stubs()->GetLookupPropertyStub());

  __ IsNil(eax, NULL, &done);
  __ mov(ebx, *inputs[0]->ToOperand());
  Operand qmap(ebx, HObject::kMapOffset);
  __ mov(ebx, qmap);
  __ addl(eax, ebx);

  Operand slot(eax, 0);
  __ mov(eax, slot);

  __ bind(&done);
  __ mov(*result->ToOperand(), eax);
}


void FDeleteProperty::Generate(Masm* masm) {
  // eax <- object
  // ebx <- property
  __ mov(eax, *inputs[0]->ToOperand());
  __ mov(ebx, *inputs[1]->ToOperand());
  __ Call(masm->stubs()->GetDeletePropertyStub());
}


void FAllocateObject::Generate(Masm* masm) {
  __ pushb(Immediate(HNumber::Tag(Heap::kTagNil)));
  __ pushb(Immediate(HNumber::Tag(Heap::kTagNil)));
  __ push(Immediate(HNumber::Tag(size_)));
  __ pushb(Immediate(HNumber::Tag(Heap::kTagObject)));
  __ Call(masm->stubs()->GetAllocateObjectStub());
  __ addlb(esp, Immediate(4 * 4));
  __ mov(*result->ToOperand(), eax);
}


void FAllocateArray::Generate(Masm* masm) {
  __ pushb(Immediate(HNumber::Tag(Heap::kTagNil)));
  __ pushb(Immediate(HNumber::Tag(Heap::kTagNil)));
  __ push(Immediate(HNumber::Tag(size_)));
  __ pushb(Immediate(HNumber::Tag(Heap::kTagArray)));
  __ Call(masm->stubs()->GetAllocateObjectStub());
  __ addlb(esp, Immediate(4 * 4));
  __ mov(*result->ToOperand(), eax);
}


void FIf::Generate(Masm* masm) {
  __ mov(eax, *inputs[0]->ToOperand());
  __ Call(masm->stubs()->GetCoerceToBooleanStub());

  // Jmp to `right` block if value is `false`
  Operand bvalue(eax, HBoolean::kValueOffset);
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
  // eax <- value
  __ mov(eax, *inputs[0]->ToOperand());

  // Coerce value to boolean first
  __ Call(masm->stubs()->GetCoerceToBooleanStub());

  Label on_false, done;

  Heap* heap = masm->heap();
  Immediate root(reinterpret_cast<intptr_t>(heap->old_space()->root()));
  Operand scratch_op(scratch, 0);
  __ mov(scratch, root);
  __ mov(scratch, scratch_op);

  // Jmp to `right` block if value is `false`
  Operand bvalue(eax, HBoolean::kValueOffset);
  __ cmpb(bvalue, Immediate(0));
  __ jmp(kEq, &on_false);

  Operand truev(scratch, HContext::GetIndexDisp(Heap::kRootTrueIndex));
  Operand falsev(scratch, HContext::GetIndexDisp(Heap::kRootFalseIndex));

  // !true = false
  __ mov(eax, falsev);

  __ jmp(&done);
  __ bind(&on_false);

  // !false = true
  __ mov(eax, truev);

  __ bind(&done);

  // result -> eax
  __ mov(*result->ToOperand(), eax);
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

  // eax <- lhs
  // ebx <- rhs
  __ mov(eax, *inputs[0]->ToOperand());
  __ mov(ebx, *inputs[1]->ToOperand());
  __ Call(stub);
  // result -> eax
  __ mov(*result->ToOperand(), eax);
}

#undef BINARY_SUB_TYPES
#undef BINARY_SUB_ENUM

void FFunction::Generate(Masm* masm) {
  // Get function's body address from relocation info
  __ mov(scratch, Immediate(0));
  RelocationInfo* addr = new RelocationInfo(RelocationInfo::kAbsolute,
                                            RelocationInfo::kLong,
                                            masm->offset() - 4);
  body->label.AddUse(masm, addr);

  // Call stub
  __ push(scratch);
  __ push(scratch);
  __ push(Immediate(HNumber::Tag(argc_)));
  __ push(scratch);
  __ Call(masm->stubs()->GetAllocateFunctionStub());
  __ addlb(esp, Immediate(4 * 4));
  __ mov(*result->ToOperand(), eax);
}


void FClone::Generate(Masm* masm) {
  __ mov(eax, *inputs[0]->ToOperand());
  __ Call(masm->stubs()->GetCloneObjectStub());
  __ mov(*result->ToOperand(), eax);
}


void FSizeof::Generate(Masm* masm) {
  __ mov(eax, *inputs[0]->ToOperand());
  __ Call(masm->stubs()->GetSizeofStub());
  __ mov(*result->ToOperand(), eax);
}


void FKeysof::Generate(Masm* masm) {
  __ mov(eax, *inputs[0]->ToOperand());
  __ Call(masm->stubs()->GetKeysofStub());
  __ mov(*result->ToOperand(), eax);
}


void FTypeof::Generate(Masm* masm) {
  __ mov(eax, *inputs[0]->ToOperand());
  __ Call(masm->stubs()->GetTypeofStub());
  __ mov(*result->ToOperand(), eax);
}


void FStoreArg::Generate(Masm* masm) {
  // Calculate slot position
  __ mov(eax, esp);
  __ mov(ebx, *inputs[1]->ToOperand());
  __ shl(ebx, Immediate(1));
  __ addl(eax, ebx);
  Operand slot(eax, 0);
  __ mov(scratch, *inputs[0]->ToOperand());
  __ mov(slot, scratch);
}


void FStoreVarArg::Generate(Masm* masm) {
  // eax <- value
  // edx <- offset
  __ mov(eax, *inputs[0]->ToOperand());
  __ mov(edx, esp);
  __ shl(eax, Immediate(1));
  __ addl(edx, *inputs[1]->ToOperand());
  __ Call(masm->stubs()->GetStoreVarArgStub());
}


void FAlignStack::Generate(Masm* masm) {
  Label even, loop;
  __ mov(eax, *inputs[0]->ToOperand());
  __ bind(&loop);
  __ testb(eax, Immediate(HNumber::Tag(3)));
  __ jmp(kEq, &even);
  __ pushb(Immediate(Heap::kTagNil));
  __ addlb(eax, Immediate(HNumber::Tag(1)));
  __ jmp(&loop);
  __ bind(&even);

  // Now allocate space on-stack for arguments
  __ mov(eax, *inputs[0]->ToOperand());
  __ shl(eax, Immediate(1));
  __ subl(esp, eax);
}


void FLoadArg::Generate(Masm* masm) {
  Operand slot(scratch, 0);

  Label oob, skip;

  // NOTE: input is aligned number
  __ mov(scratch, *inputs[0]->ToOperand());

  // Check if we're trying to get argument that wasn't passed in
  Operand argc(ebp, -HValue::kPointerSize * 2);
  __ cmpl(scratch, argc);
  __ jmp(kGe, &oob);

  __ addlb(scratch, Immediate(HNumber::Tag(2)));
  __ shl(scratch, Immediate(1));
  __ addl(scratch, ebp);
  __ mov(eax, slot);
  __ mov(*result->ToOperand(), eax);

  __ jmp(&skip);
  __ bind(&oob);

  __ mov(*result->ToOperand(), Immediate(Heap::kTagNil));

  __ bind(&skip);
}


void FLoadVarArg::Generate(Masm* masm) {
  __ mov(eax, *inputs[0]->ToOperand());
  __ mov(ebx, *inputs[1]->ToOperand());
  __ mov(ecx, *inputs[2]->ToOperand());
  __ Call(masm->stubs()->GetLoadVarArgStub());
}


void FCall::Generate(Masm* masm) {
  Label not_function, even_argc, done;

  __ mov(ebx, *inputs[0]->ToOperand());
  __ mov(eax, *inputs[1]->ToOperand());

  Heap* heap = masm->heap();
  Immediate root(reinterpret_cast<intptr_t>(heap->old_space()->root()));
  Operand scratch_op(scratch, 0);

  // argc * 2
  __ mov(ecx, eax);

  __ testb(ecx, Immediate(HNumber::Tag(3)));
  __ jmp(kEq, &even_argc);
  __ orlb(ecx, Immediate(HNumber::Tag(3)));
  __ addlb(ecx, Immediate(HNumber::Tag(1)));
  __ bind(&even_argc);
  __ shl(ecx, Immediate(1));
  __ addl(ecx, esp);
  Masm::Spill esp_s(masm, ecx);

  // eax <- argc
  // ebx <- fn

  __ IsUnboxed(ebx, NULL, &not_function);
  __ IsNil(ebx, NULL, &not_function);
  __ IsHeapObject(Heap::kTagFunction, ebx, &not_function, NULL);

  Masm::Spill context_s(masm, context_reg);
  Masm::Spill root_s(masm);

  __ mov(scratch, root);
  __ mov(scratch, scratch_op);
  root_s.SpillReg(scratch);

  // eax <- argc
  // scratch <- fn
  __ CallFunction(ebx);

  // Restore context and root
  context_s.Unspill();
  root_s.Unspill(ebx);
  __ mov(scratch, root);
  __ mov(scratch_op, ebx);

  // Reset all registers to nil
  __ mov(scratch, Immediate(Heap::kTagNil));
  __ mov(ecx, scratch);
  __ mov(edx, scratch);

  __ jmp(&done);
  __ bind(&not_function);

  __ mov(eax, Immediate(Heap::kTagNil));

  __ bind(&done);
  __ mov(*result->ToOperand(), eax);

  // Unwind all arguments pushed on stack
  esp_s.Unspill(esp);
}


void FCollectGarbage::Generate(Masm* masm) {
  __ Call(masm->stubs()->GetCollectGarbageStub());
}


void FGetStackTrace::Generate(Masm* masm) {
  AbsoluteAddress addr;

  addr.Target(masm, masm->offset());

  // Pass ip
  __ mov(eax, Immediate(0));
  addr.Use(masm, masm->offset() - 4);
  __ Call(masm->stubs()->GetStackTraceStub());

  __ mov(*result->ToOperand(), eax);
}

} // namespace internal
} // namespace candor
