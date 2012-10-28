#include "lir.h"
#include "lir-inl.h"
#include "lir-instructions.h"
#include "lir-instructions-inl.h"
#include "macroassembler.h"
#include "stubs.h" // Stubs
#include <unistd.h> // intptr_t

namespace candor {
namespace internal {

// Masm helpers

Register LUse::ToRegister() {
  assert(is_register());
  return RegisterByIndex(interval()->index());
}


Operand* LUse::ToOperand() {
  assert(is_stackslot());
  return new Operand(rbp, HValue::kPointerSize * (interval()->index() + 2));
}

#define __ masm->

void LLabel::Generate(Masm* masm) {
  __ bind(&this->label);
}

void LEntry::Generate(Masm* masm) {
  __ push(rbp);
  __ mov(rbp, rsp);

  // Allocate spills
  __ AllocateSpills();

  // Allocate context slots
  __ AllocateContext(context_slots_);
}


void LReturn::Generate(Masm* masm) {
  __ mov(rsp, rbp);
  __ pop(rbp);
  __ ret(0);
}


void LNop::Generate(Masm* masm) {
  // No need to generate real nops, they're only clobbering alignment
}


void LMove::Generate(Masm* masm) {
  // Ignore nop moves
  if (result->IsEqual(inputs[0])) return;
  __ Move(result, inputs[0]);
}


void LPhi::Generate(Masm* masm) {
  // Phi is absolutely the same thing as Move
  // (it's here just for semantic meaning)
  if (result->IsEqual(inputs[0])) return;
  __ Move(result, inputs[0]);
}


void LGap::Generate(Masm* masm) {
  // Resolve loops
  Resolve();

  PairList::Item* head = pairs_.head();
  for (; head != NULL; head = head->next()) {
    Pair* p = head->value();
    __ Move(p->dst_->Use(LUse::kAny, this), p->src_->Use(LUse::kAny, this));
  }
}


void LNil::Generate(Masm* masm) {
  __ Move(result, Immediate(Heap::kTagNil));
}


void LLiteral::Generate(Masm* masm) {
  if (root_slot_->is_immediate()) {
    __ Move(result,
            Immediate(reinterpret_cast<intptr_t>(root_slot_->value())));
  } else {
    assert(root_slot_->is_context());
    assert(root_slot_->depth() == -2);
    Operand slot(root_reg, HContext::GetIndexDisp(root_slot_->index()));
    __ Move(result, slot);
  }
}


void LAllocateObject::Generate(Masm* masm) {
  // XXX Use correct size here
  __ push(Immediate(HNumber::Tag(16)));
  __ push(Immediate(HNumber::Tag(Heap::kTagObject)));
  __ Call(masm->stubs()->GetAllocateObjectStub());
}


void LAllocateArray::Generate(Masm* masm) {
  // XXX Use correct size here
  __ push(Immediate(HNumber::Tag(16)));
  __ push(Immediate(HNumber::Tag(Heap::kTagArray)));
  __ Call(masm->stubs()->GetAllocateObjectStub());
}


void LGoto::Generate(Masm* masm) {
  __ jmp(&TargetAt(0)->label);
}


void LBranch::Generate(Masm* masm) {
  // Coerce value to boolean first
  __ Call(masm->stubs()->GetCoerceToBooleanStub());

  // Jmp to `right` block if value is `false`
  Operand bvalue(rax, HBoolean::kValueOffset);
  __ cmpb(bvalue, Immediate(0));
  __ jmp(kEq, &TargetAt(1)->label);
}


void LLoadProperty::Generate(Masm* masm) {
  __ push(rax);
  __ push(rax);

  // rax <- object
  // rbx <- property
  __ mov(rcx, Immediate(0));
  __ Call(masm->stubs()->GetLookupPropertyStub());

  Label done;

  __ pop(rbx);
  __ pop(rbx);

  __ IsNil(rax, NULL, &done);
  Operand qmap(rbx, HObject::kMapOffset);
  __ mov(rbx, qmap);
  __ addq(rax, rbx);

  Operand slot(rax, 0);
  __ mov(rax, slot);

  __ bind(&done);
  __ Move(result, rax);
}


void LStoreProperty::Generate(Masm* masm) {
  __ push(rcx);
  __ push(rax);

  // rax <- object
  // rbx <- property
  // rcx <- value
  __ mov(rcx, Immediate(1));
  __ Call(masm->stubs()->GetLookupPropertyStub());

  // Make rax look like unboxed number to GC
  __ dec(rax);
  __ CheckGC();
  __ inc(rax);

  Label done;

  __ Pop(rbx);
  __ Pop(rcx);

  __ IsNil(rax, NULL, &done);
  Operand qmap(rbx, HObject::kMapOffset);
  __ mov(rbx, qmap);
  __ addq(rax, rbx);

  Operand slot(rax, 0);
  __ mov(slot, rcx);

  __ bind(&done);
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


void LBinOp::Generate(Masm* masm) {
  char* stub = NULL;

  switch (HIRBinOp::Cast(hir())->binop_type()) {
   BINARY_SUB_TYPES(BINARY_SUB_ENUM)
   default: UNEXPECTED
  }

  assert(stub != NULL);

  // rax <- lhs
  // rbx <- rhs
  __ Call(stub);
  // result -> rax
}


#undef BINARY_SUB_ENUM
#undef BINARY_SUB_TYPES


void LFunction::Generate(Masm* masm) {
}


void LLoadContext::Generate(Masm* masm) {
}


void LStoreContext::Generate(Masm* masm) {
}


void LDeleteProperty::Generate(Masm* masm) {
}


void LNot::Generate(Masm* masm) {
}


void LTypeof::Generate(Masm* masm) {
}


void LSizeof::Generate(Masm* masm) {
}


void LKeysof::Generate(Masm* masm) {
}


void LClone::Generate(Masm* masm) {
}


void LCall::Generate(Masm* masm) {
}


void LCollectGarbage::Generate(Masm* masm) {
}


void LGetStackTrace::Generate(Masm* masm) {
}


void LLoadArg::Generate(Masm* masm) {
}

} // namespace internal
} // namespace candor
