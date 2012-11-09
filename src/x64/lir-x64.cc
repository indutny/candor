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
  __ bind(this->label);
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
    __ Move(p->dst_, p->src_);
  }
}


void LNil::Generate(Masm* masm) {
  if (result->instr() == this) return;

  __ Move(result, Immediate(Heap::kTagNil));
}


void LLiteral::Generate(Masm* masm) {
  if (result->instr() == this) return;

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
  __ push(Immediate(HNumber::Tag(size_)));
  __ push(Immediate(HNumber::Tag(Heap::kTagObject)));
  __ Call(masm->stubs()->GetAllocateObjectStub());
}


void LAllocateArray::Generate(Masm* masm) {
  __ push(Immediate(HNumber::Tag(size_)));
  __ push(Immediate(HNumber::Tag(Heap::kTagArray)));
  __ Call(masm->stubs()->GetAllocateObjectStub());
}


void LGoto::Generate(Masm* masm) {
  __ jmp(TargetAt(0)->label);
}


void LBranch::Generate(Masm* masm) {
  // Coerce value to boolean first
  __ Call(masm->stubs()->GetCoerceToBooleanStub());

  // Jmp to `right` block if value is `false`
  Operand bvalue(rax, HBoolean::kValueOffset);
  __ cmpb(bvalue, Immediate(0));
  __ jmp(kEq, TargetAt(1)->label);
}


void LBranchNumber::Generate(Masm* masm) {
  Register reg = inputs[0]->ToRegister();
  Label heap_number, done;

  __ IsUnboxed(reg, &heap_number, NULL);

  __ cmpq(reg, Immediate(HNumber::Tag(0)));
  __ jmp(kEq, TargetAt(1)->label);
  __ jmp(TargetAt(0)->label);

  __ bind(&heap_number);
  Operand value(reg, HNumber::kValueOffset);
  __ movd(xmm1, value);
  __ xorqd(xmm2, xmm2);
  __ ucomisd(xmm1, xmm2);
  __ jmp(kEq, TargetAt(1)->label);

  __ bind(&done);
}


void LAccessProperty::CheckIC(Masm* masm, Label* done) {
  if (HasMonomorphicProperty()) {
    Register tmp0 = scratches[0]->ToRegister();
    Register tmp1 = scratches[1]->ToRegister();
    Operand tmp0_op(tmp0, 0);
    Operand tmp1_op(tmp1, 0);

    Label ic_miss, ic_proto_miss;

    // If object's map's proto is the same as it was in previous instruction
    // call there's a big probability that property is till in the same place.
    __ IsNil(rax, NULL, &ic_miss);
    __ IsUnboxed(rax, NULL, &ic_miss);
    __ IsHeapObject(Heap::kTagObject, rax, &ic_miss, NULL);

    Operand map_op(rax, HObject::kMapOffset);
    Operand proto_op(tmp0, HMap::kProtoOffset);
    __ mov(tmp0, map_op);
    __ mov(tmp0, proto_op);

    // IC's data
    while (masm->offset() % 4 != 2) __ nop();
    __ mov(tmp1, Immediate(Heap::kICZapValue));
    proto_ic.Target(masm, masm->offset() - 8);

    // Check if IC is disabled
    __ cmpq(tmp1, Immediate(Heap::kICDisabledValue));
    __ jmp(kEq, &ic_miss);

    // Check if proto is the same
    __ cmpq(tmp0, tmp1);
    __ jmp(kNe, &ic_proto_miss);

    // IC Hit
    // Get value from cache
    while (masm->offset() % 4 != 2) __ nop();
    __ mov(tmp1, Immediate(0));
    value_offset_ic.Target(masm, masm->offset() - 8);
    __ IsNil(tmp1, NULL, &ic_miss);

    // Return value
    __ mov(rax, tmp1);
    __ jmp(done);

    // Update IC on miss
    __ bind(&ic_proto_miss);

    Operand ic_op(tmp1, 0);
    __ mov(tmp1, Immediate(0));
    proto_ic.Use(masm, masm->offset() - 8);
    proto_ic.NotifyGC();

    __ mov(ic_op, tmp0);
    __ cmpq(tmp0, Immediate(Heap::kICDisabledValue));
    __ jmp(kNe, &ic_miss);

    // IC was disabled - nullify cache updating code

    __ mov(tmp0, Immediate(0));
    invalidate_ic.Use(masm, masm->offset() - 8);
    __ mov(tmp1, Immediate(0x9090909090909090));
    __ mov(tmp0_op, tmp1);
    __ addq(tmp0, Immediate(8));
    __ mov(tmp0_op, tmp1);
    __ addq(tmp0, Immediate(8));
    __ mov(tmp0_op, tmp1);

    __ bind(&ic_miss);
  }
}


void LAccessProperty::UpdateIC(Masm* masm) {
  if (HasMonomorphicProperty()) {
    Register tmp0 = scratches[0]->ToRegister();
    Operand tmp0_op(tmp0, 0);

    // Store address of value in IC
    // (and store addres of this two instructions to nop them later)
    while (masm->offset() % 4 != 0) __ nop();
    invalidate_ic.Target(masm, masm->offset());
    __ mov(tmp0, Immediate(0));
    value_offset_ic.Use(masm, masm->offset() - 8);
    __ mov(tmp0_op, rax);
    __ xorq(tmp0, tmp0);
    __ xorq(tmp0, tmp0);
    __ nop();
  }
}


void LLoadProperty::Generate(Masm* masm) {
  AbsoluteAddress addr;

  // Pass ip of stub's address
  __ mov(rdx, Immediate(0));
  addr.Use(masm, masm->offset() - 8);

  while (masm->offset() % 2 != 0) __ nop();
  addr.Target(masm, masm->offset() + 2);

  // rax <- object
  // rbx <- property
  __ Call(masm->stubs()->GetLoadPropertyStub());
}


void LStoreProperty::Generate(Masm* masm) {
  AbsoluteAddress addr;

  // Pass ip of stub's address
  __ mov(rdx, Immediate(0));
  addr.Use(masm, masm->offset() - 8);

  while (masm->offset() % 2 != 0) __ nop();
  addr.Target(masm, masm->offset() + 2);

  // rax <- object
  // rbx <- property
  // rcx <- value
  __ Call(masm->stubs()->GetStorePropertyStub());
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


void LBinOpNumber::Generate(Masm* masm) {
  BinOp::BinOpType type = HIRBinOp::Cast(hir())->binop_type();

  Register left = rax;
  Register right = rbx;
  Register scratch = scratches[0]->ToRegister();
  Label stub_call, done;

  __ IsUnboxed(left, &stub_call, NULL);
  __ IsUnboxed(right, &stub_call, NULL);

  // Save left side in case of overflow
  __ mov(scratch, left);

  switch (type) {
   case BinOp::kAdd:
    __ addq(left, right);
    break;
   case BinOp::kSub:
    __ subq(left, right);
    break;
   case BinOp::kMul:
    __ Untag(left);
    __ imulq(right);
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

  // rax <- lhs
  // rbx <- rhs
  __ Call(stub);
  // result -> rax

  __ bind(&done);
}

#undef BINARY_SUB_ENUM
#undef BINARY_SUB_TYPES

void LFunction::Generate(Masm* masm) {
  // Get function's body address from relocation info
  __ mov(scratches[0]->ToRegister(), Immediate(0));
  RelocationInfo* addr = new RelocationInfo(RelocationInfo::kAbsolute,
                                            RelocationInfo::kQuad,
                                            masm->offset() - 8);
  block_->label()->label->AddUse(masm, addr);

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
  AbsoluteAddress addr;

  addr.Target(masm, masm->offset());

  // Pass ip
  __ mov(rax, Immediate(0));
  addr.Use(masm, masm->offset() - 8);
  __ Call(masm->stubs()->GetStackTraceStub());
}

} // namespace internal
} // namespace candor
