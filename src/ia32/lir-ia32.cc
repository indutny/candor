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
  __ Move(result, Immediate(Heap::kTagNil));
}


void LLiteral::Generate(Masm* masm) {
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
  __ push(Immediate(Heap::kTagNil));
  __ push(Immediate(Heap::kTagNil));
  __ push(Immediate(HNumber::Tag(size_)));
  __ push(Immediate(HNumber::Tag(Heap::kTagObject)));
  __ Call(masm->stubs()->GetAllocateObjectStub());
  __ addl(esp, Immediate(4 * 4));
}


void LAllocateArray::Generate(Masm* masm) {
  // XXX Use correct size here
  __ push(Immediate(Heap::kTagNil));
  __ push(Immediate(Heap::kTagNil));
  __ push(Immediate(HNumber::Tag(size_)));
  __ push(Immediate(HNumber::Tag(Heap::kTagArray)));
  __ Call(masm->stubs()->GetAllocateObjectStub());
  __ addl(esp, Immediate(4 * 4));
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


void LAccessProperty::CheckIC(Masm* masm, Label* done) {
  if (HasMonomorphicProperty()) {
    Register tmp0 = scratches[0]->ToRegister();
    Register tmp1 = scratch;
    Operand tmp0_op(tmp0, 0);
    Operand tmp1_op(tmp1, 0);

    Label ic_miss, ic_proto_miss;

    // If object's map's proto is the same as it was in previous instruction
    // call there's a big probability that property is till in the same place.
    __ IsNil(eax, NULL, &ic_miss);
    __ IsUnboxed(eax, NULL, &ic_miss);
    __ IsHeapObject(Heap::kTagObject, eax, &ic_miss, NULL);

    Operand map_op(eax, HObject::kMapOffset);
    Operand proto_op(tmp0, HMap::kProtoOffset);
    __ mov(tmp0, map_op);
    __ mov(tmp0, proto_op);

    // IC's data
    while (masm->offset() % 4 != 3) __ nop();
    __ mov(tmp1, Immediate(Heap::kICZapValue));
    proto_ic.Target(masm->offset() - 4);

    // Check if IC is disabled
    __ cmpl(tmp1, Immediate(Heap::kICDisabledValue));
    __ jmp(kEq, &ic_miss);

    // Check if proto is the same
    __ cmpl(tmp0, tmp1);
    __ jmp(kNe, &ic_proto_miss);

    // IC Hit
    // Get value from cache
    while (masm->offset() % 4 != 3) __ nop();
    __ mov(tmp1, Immediate(0));
    value_offset_ic.Target(masm->offset() - 4);
    __ IsNil(tmp1, NULL, &ic_miss);

    // Return value
    __ mov(eax, tmp1);
    __ jmp(done);

    // Update IC on miss
    __ bind(&ic_proto_miss);

    Operand ic_op(tmp1, 0);
    __ mov(tmp1, Immediate(0));
    proto_ic.Use(masm, masm->offset() - 4);
    proto_ic.NotifyGC();

    __ mov(ic_op, tmp0);

    __ bind(&ic_miss);
  }
}


void LAccessProperty::UpdateIC(Masm* masm) {
  if (HasMonomorphicProperty()) {
    Register tmp0 = scratches[0]->ToRegister();
    Operand tmp0_op(tmp0, 0);

    // Store address of value in IC
    __ mov(tmp0, Immediate(0));
    value_offset_ic.Use(masm, masm->offset() - 4);
    __ mov(tmp0_op, eax);
  }
}


void LLoadProperty::Generate(Masm* masm) {
  Label ic_done, done;

  __ push(eax);
  __ push(eax);
  __ push(eax);
  __ push(eax);

  CheckIC(masm, &ic_done);

  // eax <- object
  // ebx <- property
  __ mov(ecx, Immediate(0));
  __ Call(masm->stubs()->GetLookupPropertyStub());

  UpdateIC(masm);
  __ bind(&ic_done);

  __ pop(ebx);
  __ pop(ebx);
  __ pop(ebx);
  __ pop(ebx);

  __ IsNil(eax, NULL, &done);
  Operand qmap(ebx, HObject::kMapOffset);
  __ mov(ebx, qmap);
  __ addl(eax, ebx);

  Operand slot(eax, 0);
  __ mov(eax, slot);

  __ bind(&done);

  if (HasMonomorphicProperty()) {
    // Cleanup scratches
    Register tmp0 = scratches[0]->ToRegister();
    Register tmp1 = scratch;
    __ xorl(tmp0, tmp0);
    __ xorl(tmp1, tmp1);
  }
}


void LStoreProperty::Generate(Masm* masm) {
  Label ic_done;

  __ push(eax);
  __ push(eax);
  __ push(ecx);
  __ push(ecx);

  // eax <- object
  // ebx <- property
  __ mov(ecx, Immediate(1));

  CheckIC(masm, &ic_done);
  __ Call(masm->stubs()->GetLookupPropertyStub());
  UpdateIC(masm);
  __ bind(&ic_done);

  // Make eax look like unboxed number to GC
  __ dec(eax);
  __ CheckGC();
  __ inc(eax);

  __ pop(ecx);
  __ pop(ecx);
  __ pop(ebx);
  __ pop(ebx);

  Label done;
  __ IsNil(eax, NULL, &done);

  Operand qmap(ebx, HObject::kMapOffset);
  __ push(ebx);
  __ mov(ebx, qmap);
  __ addl(eax, ebx);
  __ pop(ebx);

  Operand slot(eax, 0);
  __ mov(slot, ecx);

  __ bind(&done);

  if (HasMonomorphicProperty()) {
    // Cleanup scratches
    Register tmp0 = scratches[0]->ToRegister();
    Register tmp1 = scratch;
    __ xorl(tmp0, tmp0);
    __ xorl(tmp1, tmp1);
  }
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
  __ push(Immediate(Heap::kTagNil));
  __ push(Immediate(Heap::kTagNil));
  __ push(Immediate(HNumber::Tag(arg_count_)));
  __ push(scratches[0]->ToRegister());
  __ Call(masm->stubs()->GetAllocateFunctionStub());
  __ addl(esp, Immediate(4 * 4));
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
  __ addl(ecx, Immediate(HNumber::Tag(1)));
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

  __ addl(scratch, Immediate(HNumber::Tag(2)));
  __ shl(scratch, 1);
  __ addl(scratch, ebp);
  __ Move(result, slot);

  __ jmp(&skip);
  __ bind(&oob);

  // NOTE: result may have the same value as input
  __ Move(result, Immediate(Heap::kTagNil));

  __ bind(&skip);
}


void LLoadVarArg::Generate(Masm* masm) {
  // offset and rest are unboxed
  Register offset = eax;
  Register rest = ebx;
  Register arr = ecx;
  Operand argc(ebp, -HValue::kPointerSize * 2);
  Operand qmap(arr, HObject::kMapOffset);
  Operand slot(scratch, 0);
  Operand stack_slot(offset, 0);

  Label loop, preloop, end;

  // Calculate length of vararg array
  __ mov(scratch, offset);
  __ addl(scratch, rest);

  // If offset + rest <= argc - return immediately
  __ cmpl(scratch, argc);
  __ jmp(kGe, &end);

  // edx = argc - offset - rest
  __ mov(edx, argc);
  __ subl(edx, scratch);

  // Array index
  __ mov(ebx, Immediate(HNumber::Tag(0)));

  Masm::Spill arr_s(masm, arr), edx_s(masm);
  Masm::Spill offset_s(masm, offset), ebx_s(masm);

  __ bind(&loop);

  // while (edx > 0)
  __ cmpl(edx, Immediate(HNumber::Tag(0)));
  __ jmp(kEq, &end);

  edx_s.SpillReg(edx);
  ebx_s.SpillReg(ebx);

  __ mov(eax, arr);

  // eax <- object
  // ebx <- property
  __ mov(ecx, Immediate(1));
  __ Call(masm->stubs()->GetLookupPropertyStub());

  arr_s.Unspill();
  ebx_s.Unspill();

  // Make eax look like unboxed number to GC
  __ dec(eax);
  __ CheckGC();
  __ inc(eax);

  __ IsNil(eax, NULL, &preloop);

  __ mov(arr, qmap);
  __ addl(eax, arr);
  __ mov(scratch, eax);

  // Get stack offset
  offset_s.Unspill();
  __ addl(offset, Immediate(HNumber::Tag(2)));
  __ addl(offset, ebx);
  __ shl(offset, 1);
  __ addl(offset, ebp);
  __ mov(offset, stack_slot);

  // Put argument in array
  __ mov(slot, offset);

  arr_s.Unspill();

  __ bind(&preloop);

  // Increment array index
  __ addl(ebx, Immediate(HNumber::Tag(1)));

  // edx --
  edx_s.Unspill();
  __ subl(edx, Immediate(HNumber::Tag(1)));
  __ jmp(&loop);

  __ bind(&end);

  // Cleanup?
  __ xorl(eax, eax);
  __ xorl(ebx, ebx);
  __ xorl(edx, edx);
  // ecx <- holds result
}


void LStoreArg::Generate(Masm* masm) {
  __ push(inputs[0]->ToRegister());
}


void LStoreVarArg::Generate(Masm* masm) {
  Register varg = eax;
  Register index = ebx;
  Register map = ecx;

  // eax <- varg
  Label loop, not_array, odd_end, r1_nil, r2_nil;
  Masm::Spill index_s(masm), map_s(masm), array_s(masm), r1(masm);
  Operand slot(eax, 0);

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
  __ cmpl(index, Immediate(HNumber::Tag(0)));
  __ jmp(kEq, &not_array);

  // index--;
  __ subl(index, Immediate(HNumber::Tag(1)));

  index_s.SpillReg(index);

  // odd case: array[index]
  __ mov(ebx, index);
  __ mov(ecx, Immediate(0));
  __ Call(masm->stubs()->GetLookupPropertyStub());

  __ IsNil(eax, NULL, &r1_nil);
  map_s.Unspill();
  __ addl(eax, map);
  __ mov(eax, slot);

  __ bind(&r1_nil);
  r1.SpillReg(eax);

  index_s.Unspill();

  // if (index == 0) goto odd_end;
  __ cmpl(index, Immediate(HNumber::Tag(0)));
  __ jmp(kEq, &odd_end);

  // index--;
  __ subl(index, Immediate(HNumber::Tag(1)));

  array_s.Unspill();
  index_s.SpillReg(index);

  // even case: array[index]
  __ mov(ebx, index);
  __ mov(ecx, Immediate(0));
  __ Call(masm->stubs()->GetLookupPropertyStub());

  __ IsNil(eax, NULL, &r2_nil);
  map_s.Unspill();
  __ addl(eax, map);
  __ mov(eax, slot);

  __ bind(&r2_nil);

  // Push two item at the same time (to preserve alignment)
  r1.Unspill(index);
  __ push(index);
  __ push(eax);

  index_s.Unspill();
  array_s.Unspill();

  __ jmp(&loop);

  __ bind(&odd_end);

  r1.Unspill(eax);
  __ push(eax);

  __ bind(&not_array);

  __ xorl(map, map);
}


void LAlignStack::Generate(Masm* masm) {
  Label even, loop;
  __ mov(scratches[0]->ToRegister(), inputs[0]->ToRegister());
  __ bind(&loop);
  __ testb(scratches[0]->ToRegister(), Immediate(HNumber::Tag(3)));
  __ jmp(kEq, &even);
  __ push(Immediate(Heap::kTagNil));
  __ addl(scratches[0]->ToRegister(), Immediate(HNumber::Tag(1)));
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

  addr.Target(masm->offset());

  // Pass ip
  __ mov(eax, Immediate(0));
  addr.Use(masm, masm->offset() - 4);
  __ Call(masm->stubs()->GetStackTraceStub());
}

} // namespace internal
} // namespace candor
