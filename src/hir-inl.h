#ifndef _SRC_HIR_INL_H_
#define _SRC_HIR_INL_H_

#include "hir.h"
#include "assert.h"

namespace candor {
namespace internal {
namespace hir {

inline void Gen::set_current_block(Block* b) {
  current_block_ = b;
}


inline void Gen::set_current_root(Block* b) {
  current_root_ = b;
}


inline Block* Gen::current_block() {
  return current_block_;
}


inline Block* Gen::current_root() {
  return current_root_;
}


inline Block* Gen::CreateBlock(int stack_slots) {
  Block* b = new Block(this);
  b->env(new Environment(stack_slots));

  blocks_.Push(b);

  return b;
}


inline Block* Gen::CreateBlock() {
  return CreateBlock(current_block()->env()->stack_slots());
}


inline Instruction* Gen::CreateInstruction(InstructionType type) {
  return new Instruction(this, current_block(), type);
}


inline Phi* Gen::CreatePhi(ScopeSlot* slot) {
  return new Phi(this, current_block(), slot);
}


inline Instruction* Gen::Add(InstructionType type) {
  return current_block()->Add(type);
}


inline Instruction* Gen::Add(InstructionType type, ScopeSlot* slot) {
  return current_block()->Add(type, slot);
}


inline Instruction* Gen::Add(Instruction* instr) {
  return current_block()->Add(instr);
}


inline Instruction* Gen::Goto(InstructionType type, Block* target) {
  return current_block()->Goto(type, target);
}


inline Instruction* Gen::Branch(InstructionType type, Block* t, Block* f) {
  return current_block()->Branch(type, t, f);
}


inline Instruction* Gen::Return(InstructionType type) {
  return current_block()->Return(type);
}


inline void Gen::Print(char* out, int32_t size) {
  PrintBuffer p(out, size);
  Print(&p);
}


inline void Gen::Print(PrintBuffer* p) {
  BlockList::Item* head = blocks_.head();
  for (; head != NULL; head = head->next()) {
    head->value()->Print(p);
  }
}


inline int Gen::block_id() {
  return block_id_++;
}


inline int Gen::instr_id() {
  return instr_id_++;
}


inline Block* Block::AddSuccessor(Block* b) {
  assert(succ_count_ < 2);
  succ_[succ_count_++] = b;

  b->AddPredecessor(this);

  return b;
}


inline void Block::AddPredecessor(Block* b) {
  assert(pred_count_ < 2);
  pred_[pred_count_++] = b;

  if (pred_count_ == 1) {
    // Fast path - copy environment
    env()->Copy(b->env());
  } else {
    PropagateValues(b);
  }
}


inline Instruction* Block::Add(InstructionType type) {
  Instruction* instr = new Instruction(g_, this, type);
  return Add(instr);
}


inline Instruction* Block::Add(InstructionType type, ScopeSlot* slot) {
  Instruction* instr = new Instruction(g_, this, type, slot);
  return Add(instr);
}


inline Instruction* Block::Add(Instruction* instr) {
  instructions_.Push(instr);

  return instr;
}


inline Instruction* Block::Goto(InstructionType type, Block* target) {
  AddSuccessor(target);
  ended_ = true;
  return Add(type);
}


inline Instruction* Block::Branch(InstructionType type, Block* t, Block* f) {
  AddSuccessor(t);
  AddSuccessor(f);
  ended_ = true;
  return Add(type);
}


inline Instruction* Block::Return(InstructionType type) {
  ended_ = true;
  return Add(type);
}


inline Block* Gen::Join(Block* b1, Block* b2) {
  Block* join = CreateBlock();

  b1->Goto(kGoto, join);
  b2->Goto(kGoto, join);

  return join;
}


inline Instruction* Gen::Assign(ScopeSlot* slot, Instruction* value) {
  return current_block()->Assign(slot, value);
}


inline bool Block::IsEnded() {
  return ended_;
}


inline bool Block::IsEmpty() {
  return instructions_.length() == 0;
}


inline Phi* Block::CreatePhi(ScopeSlot* slot) {
  return new Phi(g_, this, slot);
}


inline Environment* Block::env() {
  assert(env_ != NULL);
  return env_;
}


inline void Block::env(Environment* env) {
  env_ = env;
}


inline void Block::Print(PrintBuffer* p) {
  p->Print("# Block %d\n", id);

  InstructionList::Item* head = instructions_.head();
  for (; head != NULL; head = head->next()) {
    head->value()->Print(p);
  }

  switch (succ_count_) {
   case 1:
    p->Print("# succ: %d\n--------\n", succ_[0]->id);
    break;
   case 2:
    p->Print("# succ: %d %d\n--------\n", succ_[0]->id, succ_[1]->id);
    break;
   default:
    break;
  }
}


inline Instruction* Environment::At(int i) {
  assert(i < stack_slots_);
  return instructions_[i];
}


inline void Environment::Set(int i, Instruction* value) {
  assert(i < stack_slots_);
  instructions_[i] = value;
}


inline Phi* Environment::PhiAt(int i) {
  assert(i < stack_slots_);
  return phis_[i];
}


inline void Environment::SetPhi(int i, Phi* phi) {
  assert(i < stack_slots_);
  phis_[i] = phi;
}


inline Instruction* Environment::At(ScopeSlot* slot) {
  return At(slot->index());
}


inline void Environment::Set(ScopeSlot* slot, Instruction* value) {
  Set(slot->index(), value);
}


inline Phi* Environment::PhiAt(ScopeSlot* slot) {
  return PhiAt(slot->index());
}


inline void Environment::SetPhi(ScopeSlot* slot, Phi* phi) {
  SetPhi(slot->index(), phi);
}


inline int Environment::stack_slots() {
  return stack_slots_;
}

} // namespace hir
} // namespace internal
} // namespace candor

#endif // _SRC_HIR_INL_H_
