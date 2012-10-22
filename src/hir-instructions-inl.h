#ifndef _SRC_HIR_INSTRUCTIONS_INL_H_
#define _SRC_HIR_INSTRUCTIONS_INL_H_

#include "hir-instructions.h"

namespace candor {
namespace internal {
namespace hir {

inline HInstruction* HInstruction::AddArg(Type type) {
  HInstruction* instr = new HInstruction(g_, block_, type);
  return AddArg(instr);
}


inline HInstruction* HInstruction::AddArg(HInstruction* instr) {
  assert(instr != NULL);
  args()->Push(instr);
  instr->uses()->Push(this);

  // Chaining
  return this;
}


inline bool HInstruction::Is(Type type) {
  return type_ == type;
}


inline void HInstruction::Remove() {
  removed_ = true;
}


inline bool HInstruction::IsRemoved() {
  return removed_;
}

#define HIR_INSTRUCTION_STR(I) \
  case k##I: \
   res = #I; \
   break;

inline const char* HInstruction::TypeToStr(Type type) {
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

inline HBlock* HInstruction::block() {
  return block_;
}


inline ScopeSlot* HInstruction::slot() {
  return slot_;
}


inline void HInstruction::slot(ScopeSlot* slot) {
  slot_ = slot;
}


inline AstNode* HInstruction::ast() {
  return ast_;
}


inline void HInstruction::ast(AstNode* ast) {
  ast_ = ast;
}


inline HInstructionList* HInstruction::args() {
  return &args_;
}


inline HInstructionList* HInstruction::uses() {
  return &uses_;
}


inline void HPhi::AddInput(HInstruction* instr) {
  assert(input_count_ < 2);
  inputs_[input_count_++] = instr;

  AddArg(instr);
}


inline HInstruction* HPhi::InputAt(int i) {
  assert(i < input_count_);

  return inputs_[i];
}


inline void HPhi::Nilify() {
  assert(input_count_ == 0);
  type_ = kNil;
}


inline int HPhi::input_count() {
  return input_count_;
}


inline HPhi* HPhi::Cast(HInstruction* instr) {
  assert(instr->Is(kPhi));
  return reinterpret_cast<HPhi*>(instr);
}


inline HFunction* HFunction::Cast(HInstruction* instr) {
  assert(instr->Is(kFunction));
  return reinterpret_cast<HFunction*>(instr);
}

} // namespace hir
} // namespace internal
} // namespace candor

#endif // _SRC_HIR_INSTRUCTIONS_INL_H_
