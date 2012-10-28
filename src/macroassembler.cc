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

} // namespace internal
} // namespace candor
