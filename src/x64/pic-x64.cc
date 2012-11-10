#include "pic.h"
#include "code-space.h" // CodeSpace
#include "stubs.h" // Stubs
#include "macroassembler.h" // Masm

namespace candor {
namespace internal {

#define __ masm->

void PIC::Generate(Masm* masm) {
  __ push(rbp);
  __ mov(rbp, rsp);

  // Allocate spills
  __ AllocateSpills();

  Label miss, end;
  Operand rdx_op(rdx, 0);
  Operand proto_op(rax, HObject::kProtoOffset);
  Masm::Spill rax_s(masm, rax), rbx_s(masm, rbx);

  // Fast-case non-object
  __ IsNil(rax, NULL, &miss);
  __ IsUnboxed(rax, NULL, &miss);
  __ IsHeapObject(Heap::kTagObject, rax, &miss, NULL);

  // Load proto
  __ mov(rax, proto_op);
  __ cmpq(rax, Immediate(Heap::kICDisabledValue));
  __ jmp(kEq, &miss);

  // Load current index
  __ mov(rdx, Immediate(0));
  index_ = reinterpret_cast<intptr_t*>(static_cast<intptr_t>(
        masm->offset() - 8));

  Label cases[kMaxSize];

  for (int i = 0; i < kMaxSize; i++) {
    // Perform checks
    __ cmpq(rdx, Immediate(i * 2));
    __ jmp(kEq, &cases[i]);
  }

  // Cache failed - call runtime
  __ bind(&miss);

  rbx_s.Unspill();
  rbx_s.SpillReg(rax);
  rax_s.Unspill();
  __ Call(space_->stubs()->GetLookupPropertyStub());

  rbx_s.Unspill(rbx);
  __ cmpq(rbx, Immediate(Heap::kICDisabledValue));
  __ jmp(kEq, &end);

  // Amend PIC
  __ Pushad();

  // Miss(this, object, result)
  __ mov(rdi, Immediate(reinterpret_cast<intptr_t>(this)));
  rax_s.Unspill(rsi);
  __ mov(rdx, rax);
  MissCallback miss_cb = &Miss;
  __ mov(scratch, Immediate(*reinterpret_cast<intptr_t*>(&miss_cb)));
  __ Call(scratch);

  __ Popad(reg_nil);

  // Return value
  __ bind(&end);
  __ FinalizeSpills();
  __ xorq(rbx, rbx);
  __ mov(rsp, rbp);
  __ pop(rbp);
  __ ret(0);

  for (int i = kMaxSize - 1; i >= 0; i--) {
    Label local_miss;

    __ bind(&cases[i]);
    __ mov(rbx, Immediate(0));
    protos_[i] = reinterpret_cast<char**>(static_cast<intptr_t>(
          masm->offset() - 8));
    __ cmpq(rax, rbx);
    __ jmp(kNe, &local_miss);
    __ mov(rax, Immediate(0));
    results_[i] = reinterpret_cast<intptr_t*>(static_cast<intptr_t>(
          masm->offset() - 8));
    __ xorq(rbx, rbx);
    __ mov(rsp, rbp);
    __ pop(rbp);
    __ ret(0);
    __ bind(&local_miss);
  }
  __ jmp(&miss);
}

} // namespace internal
} // namespace candor
