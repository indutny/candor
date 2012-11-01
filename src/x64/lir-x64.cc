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
  return new Operand(rbp, -HValue::kPointerSize * (interval()->index() + 3));
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

  // Save argc
  Operand argc(rbp, -HValue::kPointerSize * 2);
  __ mov(argc, rax);

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
  // Phi is absolutely the same thing as Nop
  // (it's here just for semantic meaning)
}


void LGap::Generate(Masm* masm) {
  // Resolve loops
  Resolve();

  PairList::Item* head = pairs_.head();
  for (; head != NULL; head = head->next()) {
    Pair* p = head->value();
    __ Move(p->dst_->Use(LUse::kAny, this),
            p->src_->Use(LUse::kAny, this));
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
}


void LStoreProperty::Generate(Masm* masm) {
  __ push(rax);
  __ push(rcx);

  // rax <- object
  // rbx <- property
  // rcx <- value
  __ mov(rcx, Immediate(1));
  __ Call(masm->stubs()->GetLookupPropertyStub());

  // Make rax look like unboxed number to GC
  __ dec(rax);
  __ CheckGC();
  __ inc(rax);

  __ pop(rcx);
  __ pop(rbx);

  Label done;
  __ IsNil(rax, NULL, &done);

  Operand qmap(rbx, HObject::kMapOffset);
  __ mov(rbx, qmap);
  __ addq(rax, rbx);

  Operand slot(rax, 0);
  __ mov(slot, rcx);

  __ bind(&done);
}


void LDeleteProperty::Generate(Masm* masm) {
  // rax <- object
  // rbx <- property
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

  // rax <- lhs
  // rbx <- rhs
  __ Call(stub);
  // result -> rax
}

#undef BINARY_SUB_ENUM
#undef BINARY_SUB_TYPES

void LFunction::Generate(Masm* masm) {
  // Get function's body address from relocation info
  __ mov(scratches[0]->ToRegister(), Immediate(0));
  RelocationInfo* addr = new RelocationInfo(RelocationInfo::kAbsolute,
                                            RelocationInfo::kQuad,
                                            masm->offset() - 8);
  block_->label()->label.AddUse(masm, addr);

  // Call stub
  __ push(Immediate(HNumber::Tag(arg_count_)));
  __ push(scratches[0]->ToRegister());
  __ Call(masm->stubs()->GetAllocateFunctionStub());
}


void LCall::Generate(Masm* masm) {
  Label not_function, even_argc, done;

  // argc * 2
  __ mov(scratch, rax);

  __ testb(scratch, Immediate(HNumber::Tag(1)));
  __ jmp(kEq, &even_argc);
  __ addq(scratch, Immediate(HNumber::Tag(1)));
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

  // Unwind all arguments pushed on stack
  rsp_s.Unspill(rsp);
}


void LLoadArg::Generate(Masm* masm) {
  Operand slot(scratch, 0);

  Label oob, skip;

  // NOTE: input is aligned number
  __ mov(scratch, inputs[0]->ToRegister());

  // Check if we're trying to get argument that wasn't passed in
  Operand argc(rbp, -HValue::kPointerSize * 2);
  __ cmpq(scratch, argc);
  __ jmp(kGe, &oob);

  __ addq(scratch, Immediate(HNumber::Tag(2)));
  __ shl(scratch, 2);
  __ addq(scratch, rbp);
  __ Move(result, slot);

  __ jmp(&skip);
  __ bind(&oob);

  // NOTE: result may have the same value as input
  __ Move(result, Immediate(Heap::kTagNil));

  __ bind(&skip);
}


void LLoadVarArg::Generate(Masm* masm) {
  // offset and rest are unboxed
  Register offset = rax;
  Register rest = rbx;
  Register arr = rcx;
  Operand argc(rbp, -HValue::kPointerSize * 2);
  Operand qmap(arr, HObject::kMapOffset);
  Operand slot(scratch, 0);
  Operand stack_slot(offset, 0);

  Label loop, preloop, end;

  // Calculate length of vararg array
  __ mov(scratch, offset);
  __ addq(scratch, rest);

  // If offset + rest <= argc - return immediately
  __ cmpq(scratch, argc);
  __ jmp(kGe, &end);

  // rdx = argc - offset - rest
  __ mov(rdx, argc);
  __ subq(rdx, scratch);

  // Array index
  __ mov(rbx, Immediate(HNumber::Tag(0)));

  Masm::Spill arr_s(masm, arr), rdx_s(masm);
  Masm::Spill offset_s(masm, offset), rbx_s(masm);

  __ bind(&loop);

  // while (rdx > 0)
  __ cmpq(rdx, Immediate(HNumber::Tag(0)));
  __ jmp(kEq, &end);

  rdx_s.SpillReg(rdx);
  rbx_s.SpillReg(rbx);

  __ mov(rax, arr);

  // rax <- object
  // rbx <- property
  __ mov(rcx, Immediate(1));
  __ Call(masm->stubs()->GetLookupPropertyStub());

  arr_s.Unspill();
  rbx_s.Unspill();

  // Make rax look like unboxed number to GC
  __ dec(rax);
  __ CheckGC();
  __ inc(rax);

  __ IsNil(rax, NULL, &preloop);

  __ mov(arr, qmap);
  __ addq(rax, arr);
  __ mov(scratch, rax);

  // Get stack offset
  offset_s.Unspill();
  __ addq(offset, Immediate(HNumber::Tag(2)));
  __ addq(offset, rbx);
  __ shl(offset, 2);
  __ addq(offset, rbp);
  __ mov(offset, stack_slot);

  // Put argument in array
  __ mov(slot, offset);

  arr_s.Unspill();

  __ bind(&preloop);

  // Increment array index
  __ addq(rbx, Immediate(HNumber::Tag(1)));

  // rdx --
  rdx_s.Unspill();
  __ subq(rdx, Immediate(HNumber::Tag(1)));
  __ jmp(&loop);

  __ bind(&end);

  // Cleanup?
  __ xorq(rax, rax);
  __ xorq(rbx, rbx);
  __ xorq(rdx, rdx);
  // rcx <- holds result
}


void LStoreArg::Generate(Masm* masm) {
  __ push(inputs[0]->ToRegister());
}


void LStoreVarArg::Generate(Masm* masm) {
  Register varg = rax;
  Register index = rbx;
  Register map = rcx;

  // rax <- varg
  Label loop, not_array, odd_end, r1_nil, r2_nil;
  Masm::Spill index_s(masm), map_s(masm), array_s(masm), r1(masm);
  Operand slot(rax, 0);

  __ IsUnboxed(varg, NULL, &not_array);
  __ IsNil(varg, NULL, &not_array);
  __ IsHeapObject(Heap::kTagArray, varg, &not_array, NULL);

  Operand qmap(varg, HObject::kMapOffset);
  __ mov(map, qmap);
  map_s.SpillReg(map);

  // index = sizeof(array)
  Operand qlength(varg, HArray::kLengthOffset);
  __ mov(index, qlength);
  __ TagNumber(index);

  // while ...
  __ bind(&loop);

  array_s.SpillReg(varg);

  // while ... (index != 0) {
  __ cmpq(index, Immediate(HNumber::Tag(0)));
  __ jmp(kEq, &not_array);

  // index--;
  __ subq(index, Immediate(HNumber::Tag(1)));

  index_s.SpillReg(index);

  // odd case: array[index]
  __ mov(rbx, index);
  __ mov(rcx, Immediate(0));
  __ Call(masm->stubs()->GetLookupPropertyStub());

  __ IsNil(rax, NULL, &r1_nil);
  map_s.Unspill();
  __ addq(rax, map);
  __ mov(rax, slot);

  __ bind(&r1_nil);
  r1.SpillReg(rax);

  index_s.Unspill();

  // if (index == 0) goto odd_end;
  __ cmpq(index, Immediate(HNumber::Tag(0)));
  __ jmp(kEq, &odd_end);

  // index--;
  __ subq(index, Immediate(HNumber::Tag(1)));

  array_s.Unspill();
  index_s.SpillReg(index);

  // even case: array[index]
  __ mov(rbx, index);
  __ mov(rcx, Immediate(0));
  __ Call(masm->stubs()->GetLookupPropertyStub());

  __ IsNil(rax, NULL, &r2_nil);
  map_s.Unspill();
  __ addq(rax, map);
  __ mov(rax, slot);

  __ bind(&r2_nil);

  // Push two item at the same time (to preserve alignment)
  r1.Unspill(index);
  __ push(index);
  __ push(rax);

  index_s.Unspill();
  array_s.Unspill();

  __ jmp(&loop);

  __ bind(&odd_end);

  r1.Unspill(rax);
  __ push(rax);

  __ bind(&not_array);

  __ xorq(map, map);
}


void LAlignStack::Generate(Masm* masm) {
  Label even;
  __ testb(inputs[0]->ToRegister(), Immediate(HNumber::Tag(1)));
  __ jmp(kEq, &even);
  __ push(Immediate(Heap::kTagNil));
  __ bind(&even);
}


void LLoadContext::Generate(Masm* masm) {
  int depth = slot()->depth();

  if (depth == -1) {
    // Global object lookup
    Operand global(root_reg, HContext::GetIndexDisp(Heap::kRootGlobalIndex));
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
  // rax <- value

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
  uint32_t ip = masm->offset();

  // Pass ip
  __ mov(rax, Immediate(0));
  RelocationInfo* r = new RelocationInfo(RelocationInfo::kAbsolute,
                                         RelocationInfo::kQuad,
                                         masm->offset() - 8);
  masm->relocation_info_.Push(r);
  r->target(ip);
  __ Call(masm->stubs()->GetStackTraceStub());
}

} // namespace internal
} // namespace candor
