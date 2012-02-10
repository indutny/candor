#ifndef _SRC_X64_ASSEMBLER_INL_H_
#define _SRC_X64_ASSEMBLER_INL_H_

#include "assembler-x64.h"

#include <string.h>

namespace dotlang {

inline void Assembler::emit_rex_if_high(Register src) {
  if (src.high() == 1) emitb(0x40 | 0x01);
}


inline void Assembler::emit_rexw(Register dst, Register src) {
  emitb(0x48 | dst.high() << 2 | src.high());
}


inline void Assembler::emit_modrm(Register dst, Register src) {
  emitb(0xC0 | dst.code() << 3 | src.code());
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


inline void Assembler::Grow() {
  if (offset_ + 32 < length_) return;

  char* new_buffer = new char[length_ << 1];
  memcpy(new_buffer, buffer_, length_);
  memset(new_buffer + length_, 0x90, length_);
  delete buffer_;
  buffer_ = new_buffer;
}

} // namespace dotlang

#endif // _SRC_X64_ASSEMBLER_INL_H_
