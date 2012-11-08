#include "macroassembler.h"
#include "lir.h"
#include "lir-inl.h"

namespace candor {
namespace internal {

void Masm::Move(LUse* dst, LUse* src) {
  if (src->is_register()) {
    Move(dst, src->ToRegister());
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


void Masm::Move(LUse* dst, Operand& src) {
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

} // namespace internal
} // namespace candor
