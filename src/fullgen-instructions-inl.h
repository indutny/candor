#ifndef _SRC_FULLGEN_INSTRUCTION_INL_H_
#define _SRC_FULLGEN_INSTRUCTION_INL_H_

#include "fullgen.h"
#include "fullgen-inl.h"
#include "fullgen-instructions.h"
#include <assert.h> // assert

namespace candor {
namespace internal {

inline FInstruction* FInstruction::SetResult(FOperand* op) {
  assert(result == NULL);
  result = op;

  return this;
}


inline FInstruction* FInstruction::AddArg(FOperand* op) {
  assert(input_count_ < 3);
  inputs[input_count_++] = op;

  return this;
}


inline AstNode* FInstruction::ast() {
  return ast_;
}


inline void FInstruction::ast(AstNode* ast) {
  ast_ = ast;
}


inline FInstruction::Type FInstruction::type() {
  return type_;
}

#define FULLGEN_INSTRUCTION_TYPE_TO_STR(V) \
    case k##V: return #V;

inline const char* FInstruction::TypeToStr(Type type) {
  switch (type) {
    FULLGEN_INSTRUCTION_TYPES(FULLGEN_INSTRUCTION_TYPE_TO_STR)
    default: UNEXPECTED break;
  }
  return "none";
}

#undef FULLGEN_INSTRUCTION_TYPE_TO_STR

inline AstNode* FFunction::root_ast() {
  return root_ast_;
}


inline int FEntry::stack_slots() {
  return stack_slots_;
}


inline void FEntry::stack_slots(int stack_slots) {
  stack_slots_ = stack_slots;
}

} // namespace internal
} // namespace candor

#endif // _SRC_FULLGEN_INSTRUCTION_INL_H_
