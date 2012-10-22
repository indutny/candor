#ifndef _SRC_HIR_INSTRUCTIONS_INL_H_
#define _SRC_HIR_INSTRUCTIONS_INL_H_

#include "hir-instructions.h"

namespace candor {
namespace internal {

inline HIRInstruction* HIRInstruction::AddArg(Type type) {
  HIRInstruction* instr = new HIRInstruction(g_, block_, type);
  return AddArg(instr);
}


inline HIRInstruction* HIRInstruction::AddArg(HIRInstruction* instr) {
  assert(instr != NULL);
  args()->Push(instr);
  instr->uses()->Push(this);

  // Chaining
  return this;
}


inline bool HIRInstruction::Is(Type type) {
  return type_ == type;
}


inline void HIRInstruction::Remove() {
  removed_ = true;
}


inline bool HIRInstruction::IsRemoved() {
  return removed_;
}

#define HIR_INSTRUCTION_STR(I) \
  case k##I: \
   res = #I; \
   break;

inline const char* HIRInstruction::TypeToStr(Type type) {
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

inline HIRBlock* HIRInstruction::block() {
  return block_;
}


inline ScopeSlot* HIRInstruction::slot() {
  return slot_;
}


inline void HIRInstruction::slot(ScopeSlot* slot) {
  slot_ = slot;
}


inline AstNode* HIRInstruction::ast() {
  return ast_;
}


inline void HIRInstruction::ast(AstNode* ast) {
  ast_ = ast;
}


inline HIRInstructionList* HIRInstruction::args() {
  return &args_;
}


inline HIRInstructionList* HIRInstruction::uses() {
  return &uses_;
}


inline void HIRPhi::AddInput(HIRInstruction* instr) {
  assert(input_count_ < 2);
  inputs_[input_count_++] = instr;

  AddArg(instr);
}


inline HIRInstruction* HIRPhi::InputAt(int i) {
  assert(i < input_count_);

  return inputs_[i];
}


inline void HIRPhi::Nilify() {
  assert(input_count_ == 0);
  type_ = kNil;
}


inline int HIRPhi::input_count() {
  return input_count_;
}


inline HIRPhi* HIRPhi::Cast(HIRInstruction* instr) {
  assert(instr->Is(kPhi));
  return reinterpret_cast<HIRPhi*>(instr);
}


inline HIRFunction* HIRFunction::Cast(HIRInstruction* instr) {
  assert(instr->Is(kFunction));
  return reinterpret_cast<HIRFunction*>(instr);
}

} // namespace internal
} // namespace candor

#endif // _SRC_HIR_INSTRUCTIONS_INL_H_
