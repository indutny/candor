#ifndef _SRC_X64_MARCOASSEMBLER_H_
#define _SRC_X64_MARCOASSEMBLER_H_

#include "assembler-x64.h"
#include "ast.h" // AstNode
#include "heap.h" // HeapValue

namespace dotlang {

class MValue : public AstNode {
 public:
  enum AllocationInfo {
    kNone,
    kRegister,
    kOperand,
    kImmediate
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

  inline void imm(Immediate* imm) {
    info_ = kImmediate;
    imm_ = imm;
  }

  inline bool isNone() { return info_ == kNone; }
  inline bool isOp() { return info_ == kOperand; }
  inline bool isReg() { return info_ == kRegister; }
  inline bool isImm() { return info_ == kImmediate; }

  inline Operand* op() { return op_; }
  inline Register reg() { return reg_; }
  inline Immediate* imm() { return imm_; }

 private:
  AllocationInfo info_;
  Operand* op_;
  Register reg_;
  Immediate* imm_;
};

class Masm : public Assembler {
 public:
  Masm(Heap* heap) : result_(scratch), heap_(heap) {
  }

  void Allocate(Register result,
                Register result_end,
                uint32_t size,
                Register scratch,
                Label* runtime_allocate);
  void Mov(MValue* dst, MValue* src);

  inline Register result() { return result_; }
  Register result_;

 protected:
  Heap* heap_;
};

} // namespace dotlang

#endif // _SRC_X64_MARCOASSEMBLER_H_
