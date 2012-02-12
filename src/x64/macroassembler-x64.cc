#include "macroassembler-x64.h"

namespace dotlang {

void Masm::Mov(MValue* dst, MValue* src) {
  assert(!src->isNone());
  assert(!dst->isNone());

  if (dst->isReg()) {
    if (src->isReg()) {
      movq(dst->reg(), src->reg());
    } else {
      movq(dst->reg(), src->op());
    }
  } else {
    if (src->isReg()) {
      movq(dst->op(), src->reg());
    } else {
      movq(scratch, src->op());
      movq(dst->op(), scratch);
    }
  }
}

} // namespace dotlang
