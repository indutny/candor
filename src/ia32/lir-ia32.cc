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

  // Argc and return address
  return new Operand(ebp, -HValue::kPointerSize * (interval()->index() + 3));
}

#define __ masm->

void LLabel::Generate(Masm* masm) {
  __ bind(this->label);
}

void LEntry::Generate(Masm* masm) {
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


void LReturn::Generate(Masm* masm) {
  __ mov(esp, ebp);
  __ pop(ebp);
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
  // Phi is absolutely the same thing as Nop
  // (it's here just for semantic meaning)
}


void LGap::Generate(Masm* masm) {
  // Resolve loops
  Resolve();

  PairList::Item* head = pairs_.head();
  for (; head != NULL; head = head->next()) {
    Pair* p = head->value();
    __ Move(p->dst_, p->src_);
  }
}


void LNil::Generate(Masm* masm) {
  if (result->instr() == this) return;
  __ Move(result, Immediate(Heap::kTagNil));
}


void LLiteral::Generate(Masm* masm) {
  if (result->instr() == this) return;

  Heap* heap = masm->heap();
  Immediate root(reinterpret_cast<intptr_t>(heap->old_space()->root()));
  Operand scratch_op(scratch, 0);

  if (root_slot_->is_immediate()) {
    __ Move(result,
            Immediate(reinterpret_cast<intptr_t>(root_slot_->value())));
  } else {
    assert(root_slot_->is_context());
    assert(root_slot_->depth() == -2);
    __ mov(scratch, root);
    __ mov(scratch, scratch_op);
    Operand slot(scratch, HContext::GetIndexDisp(root_slot_->index()));
    __ Move(result, slot);
  }
}


void LAllocateObject::Generate(Masm* masm) {
  // XXX Use correct size here
  __ pushb(Immediate(Heap::kTagNil));
  __ pushb(Immediate(Heap::kTagNil));
  __ push(Immediate(HNumber::Tag(size_)));
  __ pushb(Immediate(HNumber::Tag(Heap::kTagObject)));
  __ Call(masm->stubs()->GetAllocateObjectStub());
  __ addlb(esp, Immediate(4 * 4));
}


void LAllocateArray::Generate(Masm* masm) {
  // XXX Use correct size here
  __ pushb(Immediate(Heap::kTagNil));
  __ pushb(Immediate(Heap::kTagNil));
  __ push(Immediate(HNumber::Tag(size_)));
  __ pushb(Immediate(HNumber::Tag(Heap::kTagArray)));
  __ Call(masm->stubs()->GetAllocateObjectStub());
  __ addlb(esp, Immediate(4 * 4));
}


void LGoto::Generate(Masm* masm) {
  __ jmp(TargetAt(0)->label);
}


void LBranch::Generate(Masm* masm) {
  // Coerce value to boolean first
  __ Call(masm->stubs()->GetCoerceToBooleanStub());

  // Jmp to `right` block if value is `false`
  Operand bvalue(eax, HBoolean::kValueOffset);
  __ cmpb(bvalue, Immediate(0));
  __ jmp(kEq, TargetAt(1)->label);
}


void LBranchNumber::Generate(Masm* masm) {
  Register reg = inputs[0]->ToRegister();
  Label heap_number, done;

  __ IsUnboxed(reg, &heap_number, NULL);

  __ cmpl(reg, Immediate(HNumber::Tag(0)));
  __ jmp(kEq, TargetAt(1)->label);
  __ jmp(TargetAt(0)->label);

  __ bind(&heap_number);
  Operand value(reg, HNumber::kValueOffset);
  __ movd(xmm1, value);
  __ xorld(xmm2, xmm2);
  __ ucomisd(xmm1, xmm2);
  __ jmp(kEq, TargetAt(1)->label);

  __ bind(&done);
}




void LLoadProperty::Generate(Masm* masm) {
  Label done;
  Masm::Spill eax_s(masm, eax);

  // eax <- object
  // ebx <- propery
  __ mov(ecx, Immediate(0));
  if (HasMonomorphicProperty()) {
    __ Call(masm->space()->CreatePIC());
  } else {
    __ Call(masm->stubs()->GetLookupPropertyStub());
  }

  __ IsNil(eax, NULL, &done);
  eax_s.Unspill(ebx);
  Operand qmap(ebx, HObject::kMapOffset);
  __ mov(ebx, qmap);
  __ addl(eax, ebx);

  Operand slot(eax, 0);
  __ mov(eax, slot);

  __ bind(&done);
}


void LStoreProperty::Generate(Masm* masm) {
  Label done;
  Masm::Spill eax_s(masm, eax);
  Masm::Spill ecx_s(masm, ecx);

  // eax <- object
  // ebx <- propery
  // ecx <- value
  __ mov(ecx, Immediate(1));
  if (HasMonomorphicProperty()) {
    __ Call(masm->space()->CreatePIC());
  } else {
    __ Call(masm->stubs()->GetLookupPropertyStub());
  }

  // Make eax look like unboxed number to GC
  __ dec(eax);
  __ CheckGC();
  __ inc(eax);

  __ IsNil(eax, NULL, &done);
  eax_s.Unspill(ebx);
  ecx_s.Unspill(ecx);
  Operand qmap(ebx, HObject::kMapOffset);
  __ mov(ebx, qmap);
  __ addl(eax, ebx);

  Operand slot(eax, 0);
  __ mov(slot, ecx);

  // ebx <- object
  __ bind(&done);
}


void LDeleteProperty::Generate(Masm* masm) {
  // eax <- object
  // ebx <- property
  __ Call(masm->stubs()->GetDeletePropertyStub());
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

  // eax <- lhs
  // ebx <- rhs
  __ Call(stub);
  // result -> eax
}


void LBinOpNumber::Generate(Masm* masm) {
  BinOp::BinOpType type = HIRBinOp::Cast(hir())->binop_type();

  Register left = eax;
  Register right = ebx;
  Register scratch = scratches[0]->ToRegister();
  Label stub_call, done;

  __ IsUnboxed(left, &stub_call, NULL);
  __ IsUnboxed(right, &stub_call, NULL);

  // Save left side in case of overflow
  __ mov(scratch, left);

  switch (type) {
   case BinOp::kAdd:
    __ addl(left, right);
    break;
   case BinOp::kSub:
    __ subl(left, right);
    break;
   case BinOp::kMul:
    __ Untag(left);
    __ imull(right);
    break;
   default:
    UNEXPECTED
  }

  __ jmp(kNoOverflow, &done);

  // Restore left side
  __ mov(left, scratch);

  __ bind(&stub_call);

  char* stub = NULL;
  switch (type) {
   BINARY_SUB_TYPES(BINARY_SUB_ENUM)
   default: UNEXPECTED
  }
  assert(stub != NULL);

  // eax <- lhs
  // ebx <- rhs
  __ Call(stub);
  // result -> eax

  __ bind(&done);
}

#undef BINARY_SUB_ENUM
#undef BINARY_SUB_TYPES

void LFunction::Generate(Masm* masm) {
  // Get function's body address from relocation info
  __ mov(scratches[0]->ToRegister(), Immediate(0));
  RelocationInfo* addr = new RelocationInfo(RelocationInfo::kAbsolute,
                                            RelocationInfo::kLong,
                                            masm->offset() - 4);
  block_->label()->label->AddUse(masm, addr);

  // Call stub
  __ pushb(Immediate(Heap::kTagNil));
  __ pushb(Immediate(Heap::kTagNil));
  __ push(Immediate(HNumber::Tag(arg_count_)));
  __ push(scratches[0]->ToRegister());
  __ Call(masm->stubs()->GetAllocateFunctionStub());
  __ addlb(esp, Immediate(4 * 4));
}


void LCall::Generate(Masm* masm) {
  Label not_function, even_argc, done;
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

  // Unwind all arguments pushed on stack
  esp_s.Unspill(esp);
}


void LLoadArg::Generate(Masm* masm) {
  Operand slot(scratch, 0);

  Label oob, skip;

  // NOTE: input is aligned number
  __ mov(scratch, inputs[0]->ToRegister());

  // Check if we're trying to get argument that wasn't passed in
  Operand argc(ebp, -HValue::kPointerSize * 2);
  __ cmpl(scratch, argc);
  __ jmp(kGe, &oob);

  __ addlb(scratch, Immediate(HNumber::Tag(2)));
  __ shl(scratch, Immediate(1));
  __ addl(scratch, ebp);
  __ Move(result, slot);

  __ jmp(&skip);
  __ bind(&oob);

  // NOTE: result may have the same value as input
  __ Move(result, Immediate(Heap::kTagNil));

  __ bind(&skip);
}


void LLoadVarArg::Generate(Masm* masm) {
  __ Call(masm->stubs()->GetLoadVarArgStub());
}


void LStoreArg::Generate(Masm* masm) {
  __ push(inputs[0]->ToRegister());
}


void LStoreVarArg::Generate(Masm* masm) {
  __ StoreVarArg();
}


void LAlignStack::Generate(Masm* masm) {
  Label even, loop;
  __ mov(scratches[0]->ToRegister(), inputs[0]->ToRegister());
  __ bind(&loop);
  __ testb(scratches[0]->ToRegister(), Immediate(HNumber::Tag(3)));
  __ jmp(kEq, &even);
  __ pushb(Immediate(Heap::kTagNil));
  __ addlb(scratches[0]->ToRegister(), Immediate(HNumber::Tag(1)));
  __ jmp(&loop);
  __ bind(&even);
}


void LLoadContext::Generate(Masm* masm) {
  Heap* heap = masm->heap();
  Immediate root(reinterpret_cast<intptr_t>(heap->old_space()->root()));
  Operand scratch_op(scratch, 0);
  int depth = slot()->depth();

  if (depth == -1) {
    // Global object lookup
    Operand global(scratch, HContext::GetIndexDisp(Heap::kRootGlobalIndex));
    __ mov(scratch, root);
    __ mov(scratch, scratch_op);
    __ mov(result->ToRegister(), global);
    return;
  }

  __ mov(result->ToRegister(), context_reg);

  // Lookup context
  while (--depth >= 0) {
    Operand parent(result->ToRegister(), HContext::kParentOffset);
    __ mov(result->ToRegister(), parent);
  }

  Operand res(result->ToRegister(),
              HContext::GetIndexDisp(slot()->index()));
  __ mov(result->ToRegister(), res);
}


void LStoreContext::Generate(Masm* masm) {
  int depth = slot()->depth();

  // Global can't be replaced
  if (depth == -1) return;

  __ mov(scratches[0]->ToRegister(), context_reg);

  // Lookup context
  while (--depth >= 0) {
    Operand parent(scratches[0]->ToRegister(), HContext::kParentOffset);
    __ mov(scratches[0]->ToRegister(), parent);
  }

  Operand res(scratches[0]->ToRegister(),
              HContext::GetIndexDisp(slot()->index()));
  __ mov(res, inputs[0]->ToRegister());
}


void LNot::Generate(Masm* masm) {
  // eax <- value

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
}


void LTypeof::Generate(Masm* masm) {
  __ Call(masm->stubs()->GetTypeofStub());
}


void LSizeof::Generate(Masm* masm) {
  __ Call(masm->stubs()->GetSizeofStub());
}


void LKeysof::Generate(Masm* masm) {
  __ Call(masm->stubs()->GetKeysofStub());
}


void LClone::Generate(Masm* masm) {
  __ Call(masm->stubs()->GetCloneObjectStub());
}


void LCollectGarbage::Generate(Masm* masm) {
  __ Call(masm->stubs()->GetCollectGarbageStub());
}


void LGetStackTrace::Generate(Masm* masm) {
  AbsoluteAddress addr;

  addr.Target(masm, masm->offset());

  // Pass ip
  __ mov(eax, Immediate(0));
  addr.Use(masm, masm->offset() - 4);
  __ Call(masm->stubs()->GetStackTraceStub());
}

} // namespace internal
} // namespace candor
