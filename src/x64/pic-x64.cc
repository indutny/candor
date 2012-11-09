#include "pic.h"
#include "code-space.h" // CodeSpace
#include "stubs.h" // Stubs
#include "macroassembler.h" // Masm

namespace candor {
namespace internal {

#define __ masm.

char* PIC::Generate() {
  Masm masm(space_);
  __ push(rbp);
  __ mov(rbp, rsp);

  // Allocate spills
  __ AllocateSpills();

  Label miss;
  Operand rax_op(rax, 0), rbx_op(rbx, 0), rdx_op(rdx, 0);
  Operand map_op(rax, HObject::kMapOffset), proto_op(rax, HMap::kProtoOffset);
  Masm::Spill rax_s(&masm, rax), rbx_s(&masm, rbx);

  // Fast-case non-object
  __ IsNil(rax, NULL, &miss);
  __ IsUnboxed(rax, NULL, &miss);
  __ IsHeapObject(Heap::kTagObject, rax, &miss, NULL);

  // Load proto
  __ mov(rax, map_op);
  __ mov(rax, proto_op);

  // Load current index
  __ mov(rdx, Immediate(reinterpret_cast<intptr_t>(&index_)));
  __ mov(rdx, rdx_op);

  for (int i = 0; i < kMaxSize; i++) {
    Label local_miss;

    // Perform checks
    __ cmpq(rdx, Immediate(i * 2));
    __ jmp(kLe, &miss);
    __ mov(rbx, Immediate(reinterpret_cast<intptr_t>(&protos_[i])));
    __ cmpq(rax, rbx_op);
    __ jmp(kNe, &local_miss);
    __ mov(rax, Immediate(reinterpret_cast<intptr_t>(&results_[i])));
    __ mov(rax, rax_op);
    __ ret(0);
    __ bind(&local_miss);
  }

  // Cache failed - call runtime
  __ bind(&miss);

  rax_s.Unspill();
  rbx_s.Unspill();
  __ Call(space_->stubs()->GetLookupPropertyStub());

  __ FinalizeSpills();
  __ mov(rsp, rbp);
  __ pop(rbp);
  __ ret(0);

  return space_->Put(&masm);
}

} // namespace internal
} // namespace candor
