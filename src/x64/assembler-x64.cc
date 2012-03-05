#include "assembler-x64.h"
#include "assembler-x64-inl.h"

namespace candor {

void RelocationInfo::Relocate(char* buffer) {
  uint64_t addr = 0;

  if (type_ == kAbsolute) {
    addr = reinterpret_cast<uint64_t>(buffer) + target_;
  } else {
    addr = target_ - offset_;
  }

  switch (size_) {
   case kByte:
    *reinterpret_cast<uint8_t*>(buffer + offset_) = addr;
    break;
   case kWord:
    *reinterpret_cast<uint16_t*>(buffer + offset_) = addr;
    break;
   case kLong:
    *reinterpret_cast<uint32_t*>(buffer + offset_) = addr;
    break;
   case kQuad:
    *reinterpret_cast<uint64_t*>(buffer + offset_) = addr;
    break;
   default:
    break;
  }
}


void Assembler::Relocate(char* buffer) {
  List<RelocationInfo*, ZoneObject>::Item* item = relocation_info_.head();
  while (item != NULL) {
    item->value()->Relocate(buffer);
    item = item->next();
  }
}


void Assembler::Grow() {
  if (offset_ + 32 < length_) return;

  char* new_buffer = new char[length_ << 1];
  memcpy(new_buffer, buffer_, length_);
  memset(new_buffer + length_, 0xCC, length_);

  delete[] buffer_;
  buffer_ = new_buffer;
  length_ <<= 1;
}


void Assembler::nop() {
  emitb(0x90);
}


void Assembler::cpuid() {
  emitb(0x0F);
  emitb(0xA2);
}


void Assembler::push(Register src) {
  emit_rex_if_high(src);
  emitb(0x50 | src.low());
}


void Assembler::push(Immediate imm) {
  emitb(0x68);
  emitl(imm.value());
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
  label->relocate(offset());
}


void Assembler::cmpq(Register dst, Register src) {
  emit_rexw(dst, src);
  emitb(0x3B);
  emit_modrm(dst, src);
}


void Assembler::cmpq(Register dst, Operand& src) {
  emit_rexw(dst, src);
  emitb(0x3B);
  emit_modrm(dst, src);
}


void Assembler::cmpq(Register dst, Immediate src) {
  emit_rexw(dst);
  emitb(0x81);
  emit_modrm(dst, 7);
  emitl(src.value());
}


void Assembler::cmpq(Operand& dst, Immediate src) {
  emit_rexw(dst);
  emitb(0x81);
  emit_modrm(dst, 7);
  emitl(src.value());
}


void Assembler::cmpb(Register dst, Operand& src) {
  emit_rexw(dst, src);
  emitb(0x3A);
  emit_modrm(dst, src);
}


void Assembler::cmpb(Operand& dst, Immediate src) {
  emit_rexw(rax, dst);
  emitb(0x80);
  emit_modrm(dst, 7);
  emitb(src.value());
}


void Assembler::testb(Register dst, Immediate src) {
  emit_rexw(dst);
  emitb(0xF6);
  emit_modrm(dst, 0);
  emitb(src.value());
}


void Assembler::testl(Register dst, Immediate src) {
  emit_rexw(dst);
  emitb(0xF7);
  emit_modrm(dst, 0);
  emitl(src.value());
}


void Assembler::jmp(Label* label) {
  emitb(0xE9);
  emitl(0x12345678);
  label->use(offset() - 4);
}


void Assembler::jmp(Condition cond, Label* label) {
  emitb(0x0F);
  switch (cond) {
   case kEq: emitb(0x84); break;
   case kNe: emitb(0x85); break;
   case kLt: emitb(0x8C); break;
   case kLe: emitb(0x8E); break;
   case kGt: emitb(0x8F); break;
   case kGe: emitb(0x8D); break;
   case kBelow: emitb(0x82); break;
   case kBe: emitb(0x86); break;
   case kAbove: emitb(0x87); break;
   case kAe: emitb(0x83); break;
   case kCarry: emitb(0x82); break;
   case kOverflow: emitb(0x80); break;
   default:
    UNEXPECTED
  }
  emitl(0x12345678);
  label->use(offset() - 4);
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


void Assembler::movl(Operand& dst, Immediate src) {
  emitb(0xC7);
  emit_modrm(dst);
  emitl(src.value());
}


void Assembler::movb(Register dst, Immediate src) {
  emit_rexw(dst);
  emitb(0xC6);
  emit_modrm(dst, 0);
  emitb(src.value());
}


void Assembler::movb(Operand& dst, Immediate src) {
  emit_rexw(dst);
  emitb(0xC6);
  emit_modrm(dst);
  emitb(src.value());
}


void Assembler::movb(Operand& dst, Register src) {
  emit_rexw(src, dst);
  emitb(0x88);
  emit_modrm(src, dst);
}


void Assembler::addq(Register dst, Register src) {
  emit_rexw(dst, src);
  emitb(0x03);
  emit_modrm(dst, src);
}


void Assembler::addq(Register dst, Operand& src) {
  emit_rexw(dst, src);
  emitb(0x03);
  emit_modrm(dst, src);
}


void Assembler::addq(Register dst, Immediate imm) {
  emit_rexw(dst);
  emitb(0x81);
  emit_modrm(dst, 0);
  emitl(imm.value());
}


void Assembler::subq(Register dst, Register src) {
  emit_rexw(dst, src);
  emitb(0x2B);
  emit_modrm(dst, src);
}


void Assembler::subq(Register dst, Immediate src) {
  emit_rexw(dst);
  emitb(0x81);
  emit_modrm(dst, 0x05);
  emitl(src.value());
}


void Assembler::imulq(Register src) {
  emit_rexw(rax, src);
  emitb(0xF7);
  emit_modrm(src, 0x05);
}


void Assembler::idivq(Register src) {
  emit_rexw(rax, src);
  emitb(0xF7);
  emit_modrm(src, 0x07);
}


void Assembler::andq(Register dst, Register src) {
  emit_rexw(dst, src);
  emitb(0x23);
  emit_modrm(dst, src);
}


void Assembler::orq(Register dst, Register src) {
  emit_rexw(dst, src);
  emitb(0x0B);
  emit_modrm(dst, src);
}


void Assembler::orqb(Register dst, Immediate src) {
  emit_rexw(rax, dst);
  emitb(0x83);
  emit_modrm(dst, 0x01);
  emitb(src.value());
}


void Assembler::xorq(Register dst, Register src) {
  emit_rexw(dst, src);
  emitb(0x33);
  emit_modrm(dst, src);
}


void Assembler::inc(Register dst) {
  emit_rexw(rax, dst);
  emitb(0xFF);
  emit_modrm(dst, 0x00);
}


void Assembler::dec(Register dst) {
  emit_rexw(rax, dst);
  emitb(0xFF);
  emit_modrm(dst, 0x01);
}


void Assembler::shl(Register dst, Immediate src) {
  emit_rexw(rax, dst);
  emitb(0xC1);
  emit_modrm(dst, 0x04);
  emitb(src.value());
}


void Assembler::shr(Register dst, Immediate src) {
  emit_rexw(rax, dst);
  emitb(0xC1);
  emit_modrm(dst, 0x05);
  emitb(src.value());
}


void Assembler::callq(Register dst) {
  emit_rexw(rax, dst);
  emitb(0xFF);
  emit_modrm(dst, 2);
}


void Assembler::callq(Operand& dst) {
  emit_rexw(rax, dst);
  emitb(0xFF);
  emit_modrm(dst, 2);
}


// Floating point instructions


void Assembler::movqd(DoubleRegister dst, Register src) {
  emitb(0x66);
  emit_rexw(dst, src);
  emitb(0x0F);
  emitb(0x6E);
  emit_modrm(dst, src);
}


void Assembler::movqd(Register dst, DoubleRegister src) {
  emitb(0x66);
  emit_rexw(src, dst);
  emitb(0x0F);
  emitb(0x7E);
  emit_modrm(src, dst);
}


void Assembler::movqd(Operand& dst, DoubleRegister src) {
  emitb(0x66);
  emit_rexw(src, dst);
  emitb(0x0F);
  emitb(0x7E);
  emit_modrm(src, dst);
}


void Assembler::addqd(DoubleRegister dst, DoubleRegister src) {
  emitb(0xF2);
  emitb(0x0F);
  emitb(0x58);
  emit_modrm(dst, src);
}


void Assembler::subqd(DoubleRegister dst, DoubleRegister src) {
  emitb(0xF2);
  emitb(0x0F);
  emitb(0x5C);
  emit_modrm(dst, src);
}


void Assembler::mulqd(DoubleRegister dst, DoubleRegister src) {
  emitb(0xF2);
  emitb(0x0F);
  emitb(0x59);
  emit_modrm(dst, src);
}


void Assembler::divqd(DoubleRegister dst, DoubleRegister src) {
  emitb(0xF2);
  emitb(0x0F);
  emitb(0x5E);
  emit_modrm(dst, src);
}


void Assembler::xorqd(DoubleRegister dst, DoubleRegister src) {
  emitb(0x66);
  emitb(0x0F);
  emitb(0x57);
  emit_modrm(dst, src);
}


void Assembler::cvtsi2sd(DoubleRegister dst, Register src) {
  emitb(0xF2);
  emit_rexw(dst, src);
  emitb(0x0F);
  emitb(0x2A);
  emit_modrm(dst, src);
}


void Assembler::cvttsd2si(Register dst, DoubleRegister src) {
  emitb(0xF2);
  emit_rexw(dst, src);
  emitb(0x0F);
  emitb(0x2C);
  emit_modrm(dst, src);
}


void Assembler::roundsd(DoubleRegister dst,
                        DoubleRegister src,
                        RoundMode mode) {
  emitb(0x66);
  emit_rexw(dst, src);
  emitb(0x0F);
  emitb(0x3A);
  emitb(0x0B);
  emit_modrm(dst, src);

  // Exception handling mask
  emitb(static_cast<uint8_t>(mode) | 0x08);
}


void Assembler::ucomisd(DoubleRegister dst, DoubleRegister src) {
  emitb(0x66);
  emitb(0x0F);
  emitb(0x2E);
  emit_modrm(dst, src);
}

} // namespace candor
