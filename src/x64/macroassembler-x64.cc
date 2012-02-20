#include "macroassembler-x64.h"

namespace dotlang {

void Masm::Allocate(Register result,
                    Register result_end,
                    uint32_t size,
                    Register scratch1,
                    Label* runtime_allocate) {
  Immediate top(reinterpret_cast<uint64_t>(heap_->new_space()->top()));
  Immediate limit(reinterpret_cast<uint64_t>(heap_->new_space()->limit()));

  Operand scratch_op(scratch, 0);

  movq(scratch, top);
  movq(result, scratch_op);
  movq(result_end, result);

  // Add object size to the top
  addq(result_end, Immediate(size));
  jmp(kCarry, runtime_allocate);

  // Check if we exhausted buffer
  movq(scratch, limit);
  cmp(result_end, scratch_op);
  jmp(kGt, runtime_allocate);
}


void Masm::Mov(MValue* dst, MValue* src) {
  assert(!src->isNone());
  assert(!dst->isNone());
  assert(!dst->isImm());

  if (dst->isReg()) {
    if (src->isReg()) {
      movq(dst->reg(), src->reg());
    } else if (src->isOp()) {
      movq(dst->reg(), *src->op());
    } else {
      movq(dst->reg(), *src->imm());
    }
  } else if (dst->isOp()) {
    if (src->isReg()) {
      movq(*dst->op(), src->reg());
    } else if (src->isOp()) {
      movq(scratch, *src->op());
      movq(*dst->op(), scratch);
    } else {
      if (src->imm()->is64()) {
        movq(scratch, *src->imm());
        movq(*dst->op(), scratch);
      } else {
        movq(*dst->op(), *src->imm());
      }
    }
  }
}

} // namespace dotlang
