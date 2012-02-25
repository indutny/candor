#include "assembler-x64.h"
#include "assembler-x64-inl.h"

namespace dotlang {

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


void Assembler::cmp(Register dst, Immediate src) {
  emit_rexw(dst);
  emitb(0x81);
  emit_modrm(dst, 7);
  emitl(src.value());
}


void Assembler::cmp(Operand& dst, Immediate src) {
  emit_rexw(dst);
  emitb(0x81);
  emit_modrm(dst, 7);
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
   case kEq:
    emitb(0x84);
    break;
   case kNe:
    emitb(0x85);
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


void Assembler::movb(Operand& dst, Immediate src) {
  emitb(0xC6);
  emit_modrm(dst);
  emitb(src.value());
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


void Assembler::inc(Register dst) {
  emit_rexw(dst);
  emitb(0xFF);
  emit_modrm(dst, 0x00);
}


void Assembler::dec(Register dst) {
  emit_rexw(dst);
  emitb(0xFF);
  emit_modrm(dst, 0x01);
}


void Assembler::shl(Register dst, Immediate src) {
  emit_rexw(dst);
  emitb(0xC1);
  emit_modrm(dst, 0x04);
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

} // namespace dotlang
