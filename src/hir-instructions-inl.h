#ifndef _SRC_HIR_INSTRUCTIONS_INL_H_
#define _SRC_HIR_INSTRUCTIONS_INL_H_

#include "hir-instructions.h"

namespace candor {
namespace internal {
namespace hir {

inline Instruction* Instruction::AddArg(InstructionType type) {
  Instruction* instr = new Instruction(g_, block_, type);
  return AddArg(instr);
}


inline Instruction* Instruction::AddArg(Instruction* instr) {
  assert(instr != NULL);
  args_.Push(instr);
  instr->uses_.Push(this);

  // Chaining
  return this;
}


inline bool Instruction::Is(InstructionType type) {
  return type_ == type;
}

#define HIR_INSTRUCTION_STR(I) \
  case k##I: \
   res = #I; \
   break;

inline const char* Instruction::TypeToStr(InstructionType type) {
  const char* res;

  switch (type) {
    HIR_INSTRUCTION_TYPES(HIR_INSTRUCTION_STR)
   default:
    res = "none?!";
    break;
  }

  return res;
}

#undef HIR_INSTRUCTION_STR

inline Block* Instruction::block() {
  return block_;
}


inline Function* Function::Cast(Instruction* instr) {
  assert(instr->Is(kFunction));
  return reinterpret_cast<Function*>(instr);
}


inline AstNode* Function::ast() {
  return ast_;
}

} // namespace hir
} // namespace internal
} // namespace candor

#endif // _SRC_HIR_INSTRUCTIONS_INL_H_
