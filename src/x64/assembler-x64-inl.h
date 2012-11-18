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

#ifndef _SRC_X64_ASSEMBLER_INL_H_
#define _SRC_X64_ASSEMBLER_INL_H_

#include "assembler.h"

#include <assert.h>  // assert
#include <string.h>  // memcpy, memset
#include <stdlib.h>  // NULL

namespace candor {
namespace internal {

inline void Assembler::emit_rex_if_high(Register src) {
  if (src.high() == 1) emitb(0x40 | 0x01);
}


inline void Assembler::emit_rexw(Register dst) {
  emitb(0x48 | dst.high() << 2);
}


inline void Assembler::emit_rexw(const Operand& dst) {
  emitb(0x48 | dst.base().high() << 2);
}


inline void Assembler::emit_rexw(Register dst, Register src) {
  emitb(0x48 | dst.high() << 2 | src.high());
}


inline void Assembler::emit_rexw(Register dst, const Operand& src) {
  emitb(0x48 | dst.high() << 2 | src.base().high());
}


inline void Assembler::emit_rexw(const Operand& dst, Register src) {
  emitb(0x48 | dst.base().high() << 2 | src.high());
}


inline void Assembler::emit_rexw(DoubleRegister dst, Register src) {
  emitb(0x48 | dst.high() << 2 | src.high());
}


inline void Assembler::emit_rexw(DoubleRegister dst, DoubleRegister src) {
  emitb(0x48 | dst.high() << 2 | src.high());
}


inline void Assembler::emit_rexw(Register dst, DoubleRegister src) {
  emitb(0x48 | dst.high() << 2 | src.high());
}


inline void Assembler::emit_rexw(DoubleRegister dst, const Operand& src) {
  emitb(0x48 | dst.high() << 2 | src.base().high());
}


inline void Assembler::emit_modrm(Register dst) {
  emitb(0xC0 | dst.low() << 3);
}


inline void Assembler::emit_modrm(const Operand &dst) {
  if (dst.scale() == Operand::one) {
    if (dst.byte_disp()) {
      emitb(0x40 | dst.base().low());
      emitb(dst.disp());
    } else {
      emitb(0x80 | dst.base().low());
      emitl(dst.disp());
    }
  } else {
    // TODO(indutny): Support scales
  }
}


inline void Assembler::emit_modrm(Register dst, Register src) {
  emitb(0xC0 | dst.low() << 3 | src.low());
}


inline void Assembler::emit_modrm(Register dst, const Operand& src) {
  if (src.scale() == Operand::one) {
    if (src.byte_disp()) {
      emitb(0x40 | dst.low() << 3 | src.base().low());
      emitb(src.disp());
    } else {
      emitb(0x80 | dst.low() << 3 | src.base().low());
      emitl(src.disp());
    }
  } else {
  }
}


inline void Assembler::emit_modrm(Register dst, uint32_t op) {
  emitb(0xC0 | op << 3 | dst.low());
}


inline void Assembler::emit_modrm(const Operand& dst, uint32_t op) {
  if (dst.byte_disp()) {
    emitb(0x40 | op << 3 | dst.base().low());
    emitb(dst.disp());
  } else {
    emitb(0x80 | op << 3 | dst.base().low());
    emitl(dst.disp());
  }
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


inline void Assembler::emit_modrm(DoubleRegister dst, const Operand& src) {
  if (src.byte_disp()) {
    emitb(0x40 | dst.low() << 3 | src.base().low());
    emitb(src.disp());
  } else {
    emitb(0x80 | dst.low() << 3 | src.base().low());
    emitl(src.disp());
  }
}


inline void Assembler::emit_modrm(const Operand& dst, DoubleRegister src) {
  if (dst.byte_disp()) {
    emitb(0x40 | dst.base().low() | src.low() << 3);
    emitb(dst.disp());
  } else {
    emitb(0x80 | dst.base().low() | src.low() << 3);
    emitl(dst.disp());
  }
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

}  // namespace internal
}  // namespace candor

#endif  // _SRC_X64_ASSEMBLER_INL_H_
