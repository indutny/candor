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

#include "macroassembler.h"
#include "lir.h"
#include "lir-inl.h"

namespace candor {
namespace internal {

void Masm::Move(LUse* dst, LUse* src) {
  if (src->is_register()) {
    Move(dst, src->ToRegister());
  } else if (src->is_const()) {
    // Generate const load
    LInstruction* c = src->interval()->definition();
    c->result = dst;
    c->Generate(this);
  } else {
    assert(src->is_stackslot());
    Move(dst, *src->ToOperand());
  }
}


void Masm::Move(LUse* dst, Register src) {
  if (dst->is_register()) {
    mov(dst->ToRegister(), src);
  } else {
    assert(dst->is_stackslot());
    mov(*dst->ToOperand(), src);
  }
}


void Masm::Move(LUse* dst, const Operand& src) {
  if (dst->is_register()) {
    mov(dst->ToRegister(), src);
  } else {
    assert(dst->is_stackslot());
    mov(scratch, src);
    mov(*dst->ToOperand(), scratch);
  }
}


void Masm::Move(LUse* dst, Immediate src) {
  if (dst->is_register()) {
    mov(dst->ToRegister(), src);
  } else {
    assert(dst->is_stackslot());
    mov(*dst->ToOperand(), src);
  }
}


void AbsoluteAddress::Target(Masm* masm, int offset) {
  assert(ip_ == -1);
  ip_ = offset;
  if (r_ != NULL) Commit(masm);
}


void AbsoluteAddress::Use(Masm* masm, int offset) {
  assert(r_ == NULL);
  r_ = new RelocationInfo(RelocationInfo::kAbsolute,
                          RelocationInfo::kPointer,
                          offset);
  if (ip_ != -1) Commit(masm);
}


void AbsoluteAddress::Commit(Masm* masm) {
  r_->target(ip_);
  masm->relocation_info_.Push(r_);
}


void AbsoluteAddress::NotifyGC() {
  assert(r_ != NULL);
  r_->notify_gc_ = true;
}

}  // namespace internal
}  // namespace candor
