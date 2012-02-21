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


void Assembler::bind(Label* label) {
  label->relocate(pos());
}


void Assembler::cmp(Register dst, Register src) {
  emit_rexw(dst, src);
  emitb(0x3B);
  emit_modrm(dst, src);
}


void Assembler::cmp(Register dst, Operand& src) {
  emit_rexw(dst, src);
  emitb(0x3B);
  emit_modrm(dst, src);
}


void Assembler::jmp(Label* label) {
  emitb(0xE9);
  emitl(0x12345678);
  label->use(pos() - 4);
}


void Assembler::jmp(Condition cond, Label* label) {
  emitb(0x0F);
  switch (cond) {
   case kEq:
    emitb(0x84);
    break;
   case kLt:
    emitb(0x8C);
    break;
   case kLe:
    emitb(0x8E);
    break;
   case kGt:
    emitb(0x8F);
    break;
   case kGe:
    emitb(0x8D);
    break;
   case kCarry:
    emitb(0x82);
    break;
   default:
    assert(0 && "unexpected");
  }
  emitl(0x12345678);
  label->use(pos() - 4);
}


void Assembler::movq(Register dst, Register src) {
  emit_rexw(dst, src);
  emitb(0x8B);
  emit_modrm(dst, src);
}


void Assembler::movq(Register dst, Operand& src) {
  emit_rexw(dst, src);
  emitb(0x8B);
  emit_modrm(dst, src);
}


void Assembler::movq(Operand& dst, Register src) {
  emit_rexw(src, dst);
  emitb(0x89);
  emit_modrm(src, dst);
}


void Assembler::movq(Register dst, Immediate src) {
  emit_rexw(rax, dst);
  emitb(0xB8 | dst.low());
  emitq(src.value());
}


void Assembler::movq(Operand& dst, Immediate src) {
  emit_rexw(dst);
  emitb(0xC7);
  emit_modrm(dst);
  emitl(src.value());
}


void Assembler::addq(Register dst, Immediate imm) {
  emit_rexw(dst);
  emitb(0x81);
  emit_modrm(dst, 0);
  emitl(imm.value());
}


void Assembler::subq(Register dst, Immediate imm) {
  emit_rexw(dst);
  emitb(0x81);
  emit_modrm(dst, 0x05);
  emitl(imm.value());
}


void Assembler::callq(Register dst) {
  emit_rexw(rax, dst);
  emitb(0xFF);
  emit_modrm(dst, 2);
}

} // namespace dotlang
