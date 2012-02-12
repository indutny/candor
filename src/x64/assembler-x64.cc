#include "assembler-x64.h"
#include "assembler-x64-inl.h"

namespace dotlang {

void Assembler::push(Register src) {
  emit_rex_if_high(src);
  emitb(0x50 | src.low());
}


void Assembler::pop(Register dst) {
  emit_rex_if_high(dst);
  emitb(0x58 | dst.low());
}


void Assembler::ret(uint16_t imm) {
  if (imm == 0) {
    emitb(0xC3);
  } else {
    emitb(0xC2);
    emitw(imm);
  }
  Grow();
}


void Assembler::movq(Register dst, Register src) {
  emit_rexw(dst, src);
  emitb(0x8B);
  emit_modrm(dst, src);
}


void Assembler::movq(Register dst, Operand* src) {
  emit_rexw(dst, src);
  emitb(0x8B);
  emit_modrm(dst, src);
}


void Assembler::movq(Operand* dst, Register src) {
  emit_rexw(src, dst);
  emitb(0x89);
  emit_modrm(src, dst);
}


void Assembler::subq(Register dst, Immediate imm) {
  emit_rexw(dst);
  emitb(0x81);
  emit_modrm(dst, 0x05);
  emitl(imm.value());
}

} // namespace dotlang
