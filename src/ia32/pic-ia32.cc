#include "pic.h"
#include "code-space.h" // CodeSpace
#include "stubs.h" // Stubs
#include "macroassembler.h" // Masm

namespace candor {
namespace internal {

#define __ masm->

void PIC::Generate(Masm* masm) {
  __ push(ebp);
  __ mov(ebp, esp);

  // Place for spills
  __ push(Immediate(Heap::kTagNil));
  __ push(Immediate(Heap::kTagNil));

  Label miss, end;
  Operand edx_op(edx, 0);
  Operand proto_op(eax, HObject::kProtoOffset);
  Operand eax_s(ebp, -8), ebx_s(ebp, -12);

  __ mov(eax_s, eax);
  __ mov(ebx_s, ebx);

  if (size_ != 0) {
    // Proto in case of array
    __ mov(edx, Immediate(Heap::kICDisabledValue));

    // Fast-case non-object
    __ IsNil(eax, NULL, &miss);
    __ IsUnboxed(eax, NULL, &miss);
    __ IsHeapObject(Heap::kTagObject, eax, &miss, NULL);

    // Load proto
    __ mov(edx, proto_op);
    __ cmpl(edx, Immediate(Heap::kICDisabledValue));
    __ jmp(kEq, &miss);
  }

  for (int i = size_ - 1; i >= 0; i--) {
    Label local_miss;

    __ mov(ebx, Immediate(reinterpret_cast<intptr_t>(protos_[i])));
    proto_offsets_[i] = reinterpret_cast<char**>(static_cast<intptr_t>(
          masm->offset() - 4));
    __ cmpl(edx, ebx);
    __ jmp(kNe, &local_miss);
    __ mov(eax, Immediate(results_[i]));
    __ xorl(ebx, ebx);
    __ mov(esp, ebp);
    __ pop(ebp);
    __ ret(0);
    __ bind(&local_miss);
  }

  // Cache failed - call runtime
  __ bind(&miss);

  __ mov(ebx, ebx_s);
  __ mov(eax, eax_s);
  __ Call(space_->stubs()->GetLookupPropertyStub());

  // Amend PIC
  __ Pushad();

  // Miss(this, object, result, ip)
  __ mov(edi, Immediate(reinterpret_cast<intptr_t>(this)));
  __ mov(esi, eax_s);
  __ mov(edx, eax);

  Operand caller_ip(ebp, 4);
  __ mov(ecx, caller_ip);

  MissCallback miss_cb = &Miss;
  __ push(ecx);
  __ push(edx);
  __ push(esi);
  __ push(edi);
  __ mov(scratch, Immediate(*reinterpret_cast<intptr_t*>(&miss_cb)));
  __ Call(scratch);
  __ addl(esp, Immediate(4 * 4));

  __ Popad(reg_nil);

  // Return value
  __ bind(&end);
  __ xorl(ebx, ebx);
  __ mov(esp, ebp);
  __ pop(ebp);
  __ ret(0);
}

} // namespace internal
} // namespace candor
