#ifndef _SRC_HIR_INSTRUCTIONS_INL_H_
#define _SRC_HIR_INSTRUCTIONS_INL_H_

#include "hir-instructions.h"

namespace candor {
namespace internal {

inline HIRInstruction* HIRInstruction::AddArg(Type type) {
  HIRInstruction* instr = new HIRInstruction(type);
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


inline HIRInstruction::Type HIRInstruction::type() {
  return type_;
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

inline HIRInstruction::Representation HIRInstruction::representation() {
  if (representation_ == kHoleRepresentation) {
    // Prevent loops
    representation_ = kUnknownRepresentation;

    // Cache miss
    CalculateRepresentation();
  }

  return representation_;
}


inline bool HIRInstruction::IsNumber() {
  return representation() & kNumberRepresentation;
}


inline bool HIRInstruction::IsSmi() {
  return representation() & kSmiRepresentation;
}


inline bool HIRInstruction::IsHeapNumber() {
  return representation() & kHeapNumberRepresentation;
}


inline bool HIRInstruction::IsString() {
  return representation() & kStringRepresentation;
}


inline bool HIRInstruction::IsBoolean() {
  return representation() & kBooleanRepresentation;
}


inline bool HIRInstruction::IsPinned() {
  return pinned_;
}


inline HIRInstruction* HIRInstruction::Unpin() {
  pinned_ = false;
  return this;
}


inline HIRInstruction* HIRInstruction::Pin() {
  pinned_ = true;
  return this;
}


inline HIRBlock* HIRInstruction::block() {
  return block_;
}


inline void HIRInstruction::block(HIRBlock* block) {
  assert(block != NULL);
  block_ = block;
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


inline HIRInstruction* HIRInstruction::left() {
  assert(args()->length() >= 1);
  return args()->head()->value();
}


inline HIRInstruction* HIRInstruction::right() {
  assert(args()->length() >= 2);
  return args()->head()->next()->value();
}


inline HIRInstruction* HIRInstruction::third() {
  assert(args()->length() >= 3);
  return args()->head()->next()->next()->value();
}


inline LInstruction* HIRInstruction::lir() {
  return lir_;
}


inline void HIRInstruction::lir(LInstruction* lir) {
  assert(lir_ == NULL || lir_ == lir);
  lir_ = lir;
}


inline void HIRPhi::AddInput(HIRInstruction* instr) {
  assert(input_count_ < 2);
  assert(instr != NULL);
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


inline void HIRPhi::input_count(int input_count) {
  input_count_ = input_count;
}


inline ScopeSlot* HIRLiteral::root_slot() {
  return root_slot_;
}


inline int HIREntry::context_slots() {
  return context_slots_;
}


inline BinOp::BinOpType HIRBinOp::binop_type() {
  return binop_type_;
}


inline ScopeSlot* HIRLoadContext::context_slot() {
  return context_slot_;
}


inline ScopeSlot* HIRStoreContext::context_slot() {
  return context_slot_;
}


inline int HIRAllocateObject::size() {
  return size_;
}


inline int HIRAllocateArray::size() {
  return size_;
}

} // namespace internal
} // namespace candor

#endif // _SRC_HIR_INSTRUCTIONS_INL_H_
