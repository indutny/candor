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
  Label cases[kMaxSize];
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

  // Jump into correct section
  __ jmp(&miss);
  jmp_ = reinterpret_cast<uint32_t*>(static_cast<intptr_t>(
        masm->offset() - 4));

  for (int i = kMaxSize - 1; i >= 0; i--) {
    Label local_miss;

    section_size_ = masm->offset();
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
    section_size_ = masm->offset() - section_size_;
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

  // Miss(this, object, result, ip)
  __ mov(rdi, Immediate(reinterpret_cast<intptr_t>(this)));
  rax_s.Unspill(rsi);
  __ mov(rdx, rax);

  Operand caller_ip(rbp, 8);
  __ mov(rcx, caller_ip);

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
}

} // namespace internal
} // namespace candor
