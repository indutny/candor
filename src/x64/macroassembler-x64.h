#ifndef _SRC_X64_MARCOASSEMBLER_H_
#define _SRC_X64_MARCOASSEMBLER_H_

#include "assembler-x64.h"
#include "ast.h" // AstNode

namespace dotlang {

// Forward declaration
class MValue;


class Masm : public Assembler {
 public:
  void Mov(MValue* dst, MValue* src);
};


class MValue : public AstNode {
 public:
  enum AllocationInfo {
    kNone,
    kRegister,
    kOperand
  };

  MValue(): AstNode(kMValue), info_(kNone) {
  }

  MValue(AstValue* value): AstNode(kMValue), info_(kNone) {
    if (value->slot()->isStack()) {
      op(new Operand(rbp, -(value->slot()->index() + 1) * sizeof(void*)));
    } else {
      abort();
    }
    children()->Push(value);
  }

  static inline MValue* Cast(AstNode* node) {
    return reinterpret_cast<MValue*>(node);
  }

  inline void op(Operand* op) {
    info_ = kOperand;
    op_ = op;
  }

  inline void reg(Register reg) {
    info_ = kRegister;
    reg_ = reg;
  }

  inline bool isNone() { return info_ == kNone; }
  inline bool isOp() { return info_ == kOperand; }
  inline bool isReg() { return info_ == kRegister; }
  inline Operand* op() { return op_; }
  inline Register reg() { return reg_; }

 private:
  AllocationInfo info_;
  Operand* op_;
  Register reg_;
};

} // namespace dotlang

#endif // _SRC_X64_MARCOASSEMBLER_H_
