/**
 * Copyright (c) 2012, Fedor Indutny.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "pic.h"
#include "code-space.h"  // CodeSpace
#include "stubs.h"  // Stubs
#include "macroassembler.h"  // Masm

namespace candor {
namespace internal {

#define __ masm->

void PIC::Generate(Masm* masm) {
  __ push(ebp);
  __ mov(ebp, esp);

  // Place for spills
  __ pushb(Immediate(Heap::kTagNil));
  __ pushb(Immediate(Heap::kTagNil));

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

  if (size_ != 0) {
    __ mov(ebx, ebx_s);
    __ mov(eax, eax_s);
  }
  __ Call(space_->stubs()->GetLookupPropertyStub());

  // Miss(this, object, result, ip)
  Operand caller_ip(ebp, 4);
  __ push(caller_ip);
  __ push(eax);
  __ push(eax_s);
  __ push(Immediate(reinterpret_cast<intptr_t>(this)));
  __ Call(space_->stubs()->GetPICMissStub());

  // Return value
  __ bind(&end);
  __ xorl(ebx, ebx);
  __ mov(esp, ebp);
  __ pop(ebp);
  __ ret(0);
}

}  // namespace internal
}  // namespace candor
