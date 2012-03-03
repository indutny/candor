#ifndef _SRC_X64_ASSEMBLER_INL_H_
#define _SRC_X64_ASSEMBLER_INL_H_

#include "assembler-x64.h"

#include <assert.h> // assert
#include <string.h> // memcpy, memset
#include <stdlib.h> // NULL

namespace dotlang {

inline void Label::relocate(uint32_t offset) {
  // Label should be relocated only once
  assert(pos_ == 0);
  pos_ = offset - 4;

  // Iterate through all label's uses and insert correct relocation info
  List<RelocationInfo*, EmptyClass>::Item* item = uses_.head();
  while (item != NULL) {
    item->value()->target(pos_);
    item = item->next();
  }
}


inline void Label::use(uint32_t offset) {
  RelocationInfo* info = new RelocationInfo(
      RelocationInfo::kRelative,
      RelocationInfo::kLong,
      offset);
  // If we already know target position - set it
  if (pos_ != 0) info->target(pos_);
  uses_.Push(info);
  asm_->relocation_info_.Push(info);
}


inline void Assembler::emit_rex_if_high(Register src) {
  if (src.high() == 1) emitb(0x40 | 0x01);
}


inline void Assembler::emit_rexw(Register dst) {
  emitb(0x48 | dst.high() << 2);
}


inline void Assembler::emit_rexw(Operand& dst) {
  emitb(0x48 | dst.base().high() << 2);
}


inline void Assembler::emit_rexw(Register dst, Register src) {
  emitb(0x48 | dst.high() << 2 | src.high());
}


inline void Assembler::emit_rexw(Register dst, Operand& src) {
  emitb(0x48 | dst.high() << 2 | src.base().high());
}


inline void Assembler::emit_rexw(Operand& dst, Register src) {
  emitb(0x48 | dst.base().high() << 2 | src.high());
}


inline void Assembler::emit_rexw(DoubleRegister dst, Register src) {
  emitb(0x48 | dst.high() << 2 | src.high());
}


inline void Assembler::emit_rexw(Register dst, DoubleRegister src) {
  emitb(0x48 | dst.high() << 2 | src.high());
}


inline void Assembler::emit_rexw(DoubleRegister dst, Operand& src) {
  emitb(0x48 | dst.high() << 2 | src.base().high());
}


inline void Assembler::emit_modrm(Register dst) {
  emitb(0xC0 | dst.low() << 3);
}


inline void Assembler::emit_modrm(Operand &dst) {
  if (dst.scale() == Operand::one) {
    emitb(0x80 | dst.base().low());
    emitl(dst.disp());
  } else {
    // TODO: Support scales
  }
}


inline void Assembler::emit_modrm(Register dst, Register src) {
  emitb(0xC0 | dst.low() << 3 | src.low());
}


inline void Assembler::emit_modrm(Register dst, Operand& src) {
  if (src.scale() == Operand::one) {
    emitb(0x80 | dst.low() << 3 | src.base().low());
    emitl(src.disp());
  } else {
  }
}


inline void Assembler::emit_modrm(Register dst, uint32_t op) {
  emitb(0xC0 | op << 3 | dst.low());
}


inline void Assembler::emit_modrm(Operand& dst, uint32_t op) {
  emitb(0x80 | op << 3 | dst.base().low());
  emitl(dst.disp());
}


inline void Assembler::emit_modrm(DoubleRegister dst, Register src) {
  emitb(0xC0 | dst.low() << 3 | src.low());
}


inline void Assembler::emit_modrm(Register dst, DoubleRegister src) {
  emitb(0xC0 | dst.low() << 3 | src.low());
}


inline void Assembler::emit_modrm(DoubleRegister dst, DoubleRegister src) {
  emitb(0xC0 | dst.low() << 3 | src.low());
}


inline void Assembler::emit_modrm(DoubleRegister dst, Operand& src) {
  emitb(0x80 | dst.low() << 3 | src.base().low());
  emitl(src.disp());
}


inline void Assembler::emitb(uint8_t v) {
  *reinterpret_cast<uint8_t*>(pos()) = v;
  offset_ += 1;
  Grow();
}


inline void Assembler::emitw(uint16_t v) {
  *reinterpret_cast<uint16_t*>(pos()) = v;
  offset_ += 2;
  Grow();
}


inline void Assembler::emitl(uint32_t v) {
  *reinterpret_cast<uint32_t*>(pos()) = v;
  offset_ += 4;
  Grow();
}


inline void Assembler::emitq(uint64_t v) {
  *reinterpret_cast<uint64_t*>(pos()) = v;
  offset_ += 8;
  Grow();
}

} // namespace dotlang

#endif // _SRC_X64_ASSEMBLER_INL_H_
