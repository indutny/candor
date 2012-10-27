#ifndef _SRC_IA32_ASSEMBLER_INL_H_
#define _SRC_IA32_ASSEMBLER_INL_H_

#include "assembler.h"

#include <assert.h> // assert
#include <string.h> // memcpy, memset
#include <stdlib.h> // NULL

namespace candor {
namespace internal {

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


inline void Assembler::emit_modrm(Operand& dst, DoubleRegister src) {
  emitb(0x80 | dst.base().low() << 3 | src.low());
  emitl(dst.disp());
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

} // namespace internal
} // namespace candor

#endif // _SRC_IA32_ASSEMBLER_INL_H_
