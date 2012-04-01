#ifndef _SRC_X64_MARCOASSEMBLER_INL_H_
#define _SRC_X64_MARCOASSEMBLER_INL_H_

#include "macroassembler-x64.h"

namespace candor {
namespace internal {

inline void Masm::Push(Register src) {
  ChangeAlign(1);
  push(src);
}


inline void Masm::Pop(Register src) {
  pop(src);
  ChangeAlign(-1);
}


inline void Masm::PreservePop(Register src, Register preserve) {
  if (src.is(preserve)) {
    pop(scratch);
  } else {
    pop(src);
  }
}


inline void Masm::TagNumber(Register src) {
  sal(src, Immediate(1));
}


inline void Masm::Untag(Register src) {
  sar(src, Immediate(1));
}


inline Operand& Masm::SpillToOperand(int index) {
  // -1 spill is reserved for ParallelMove's use
  spill_operand_.disp(- 8 * (index + 2));
  return spill_operand_;
}


inline Condition Masm::BinOpToCondition(BinOp::BinOpType type,
                                        BinOpUsage usage) {
  if (usage == kIntegral) {
    switch (type) {
     case BinOp::kStrictEq:
     case BinOp::kEq: return kEq;
     case BinOp::kStrictNe:
     case BinOp::kNe: return kNe;
     case BinOp::kLt: return kLt;
     case BinOp::kGt: return kGt;
     case BinOp::kLe: return kLe;
     case BinOp::kGe: return kGe;
     default: UNEXPECTED
    }
  } else if (usage == kDouble) {
    switch (type) {
     case BinOp::kStrictEq:
     case BinOp::kEq: return kEq;
     case BinOp::kStrictNe:
     case BinOp::kNe: return kNe;
     case BinOp::kLt: return kBelow;
     case BinOp::kGt: return kAbove;
     case BinOp::kLe: return kBe;
     case BinOp::kGe: return kAe;
     default: UNEXPECTED
    }
  }

  // Just to shut up compiler
  return kEq;
}


inline void Masm::SpillSlot(uint32_t index, Operand& op) {
  op.base(rbp);
  op.disp(-spill_offset_ - 8 * index);
}

} // namespace internal
} // namespace candor

#endif // _SRC_X64_MARCOASSEMBLER_INL_H_
