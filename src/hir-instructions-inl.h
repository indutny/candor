#ifndef _SRC_HIR_INSTRUCTIONS_INL_H_
#define _SRC_HIR_INSTRUCTIONS_INL_H_

#include "hir-instructions.h"

namespace candor {
namespace internal {
namespace hir {

inline Instruction* Instruction::AddArg(Type type) {
  Instruction* instr = new Instruction(g_, block_, type);
  return AddArg(instr);
}


inline Instruction* Instruction::AddArg(Instruction* instr) {
  assert(instr != NULL);
  args()->Push(instr);
  instr->uses()->Push(this);

  // Chaining
  return this;
}


inline bool Instruction::Is(Type type) {
  return type_ == type;
}


inline void Instruction::Remove() {
  removed_ = true;
}


inline bool Instruction::IsRemoved() {
  return removed_;
}

#define HIR_INSTRUCTION_STR(I) \
  case k##I: \
   res = #I; \
   break;

inline const char* Instruction::TypeToStr(Type type) {
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


inline ScopeSlot* Instruction::slot() {
  return slot_;
}


inline void Instruction::slot(ScopeSlot* slot) {
  slot_ = slot;
}


inline AstNode* Instruction::ast() {
  return ast_;
}


inline void Instruction::ast(AstNode* ast) {
  ast_ = ast;
}


inline InstructionList* Instruction::args() {
  return &args_;
}


inline InstructionList* Instruction::uses() {
  return &uses_;
}


inline void Phi::AddInput(Instruction* instr) {
  assert(input_count_ < 2);
  inputs_[input_count_++] = instr;

  AddArg(instr);
}


inline Instruction* Phi::InputAt(int i) {
  assert(i < input_count_);

  return inputs_[i];
}


inline void Phi::Nilify() {
  assert(input_count_ == 0);
  type_ = kNil;
}


inline int Phi::input_count() {
  return input_count_;
}


inline Phi* Phi::Cast(Instruction* instr) {
  assert(instr->Is(kPhi));
  return reinterpret_cast<Phi*>(instr);
}


inline Function* Function::Cast(Instruction* instr) {
  assert(instr->Is(kFunction));
  return reinterpret_cast<Function*>(instr);
}

} // namespace hir
} // namespace internal
} // namespace candor

#endif // _SRC_HIR_INSTRUCTIONS_INL_H_
