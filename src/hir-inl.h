#ifndef _SRC_HIR_INL_H_
#define _SRC_HIR_INL_H_

#include "hir.h"
#include "assert.h"

namespace candor {
namespace internal {
namespace hir {

inline void HGen::set_current_block(HBlock* b) {
  current_block_ = b;
}


inline void HGen::set_current_root(HBlock* b) {
  current_root_ = b;
}


inline HBlock* HGen::current_block() {
  return current_block_;
}


inline HBlock* HGen::current_root() {
  return current_root_;
}


inline HBlock* HGen::CreateBlock(int stack_slots) {
  HBlock* b = new HBlock(this);
  b->env(new HEnvironment(stack_slots));

  blocks_.Push(b);

  return b;
}


inline HBlock* HGen::CreateBlock() {
  // NOTE: -1 for additional logic_slot
  return CreateBlock(current_block()->env()->stack_slots() - 1);
}


inline HInstruction* HGen::CreateInstruction(HInstruction::Type type) {
  return new HInstruction(this, current_block(), type);
}


inline HPhi* HGen::CreatePhi(ScopeSlot* slot) {
  return current_block()->CreatePhi(slot);
}


inline HInstruction* HGen::Add(HInstruction::Type type) {
  return current_block()->Add(type);
}


inline HInstruction* HGen::Add(HInstruction::Type type, ScopeSlot* slot) {
  return current_block()->Add(type, slot);
}


inline HInstruction* HGen::Add(HInstruction* instr) {
  return current_block()->Add(instr);
}


inline HInstruction* HGen::Goto(HInstruction::Type type, HBlock* target) {
  return current_block()->Goto(type, target);
}


inline HInstruction* HGen::Branch(HInstruction::Type type, HBlock* t, HBlock* f) {
  return current_block()->Branch(type, t, f);
}


inline HInstruction* HGen::Return(HInstruction::Type type) {
  return current_block()->Return(type);
}


inline void HGen::Print(char* out, int32_t size) {
  PrintBuffer p(out, size);
  Print(&p);
}


inline void HGen::Print(PrintBuffer* p) {
  HBlockList::Item* head = blocks_.head();
  for (; head != NULL; head = head->next()) {
    head->value()->Print(p);
  }
}


inline int HGen::block_id() {
  return block_id_++;
}


inline int HGen::instr_id() {
  int r = instr_id_;

  instr_id_ += 2;

  return r;
}


inline HBlock* HBlock::AddSuccessor(HBlock* b) {
  assert(succ_count_ < 2);
  succ_[succ_count_++] = b;

  b->AddPredecessor(this);

  return b;
}


inline HInstruction* HBlock::Add(HInstruction::Type type) {
  HInstruction* instr = new HInstruction(g_, this, type);
  return Add(instr);
}


inline HInstruction* HBlock::Add(HInstruction::Type type, ScopeSlot* slot) {
  HInstruction* instr = new HInstruction(g_, this, type, slot);
  return Add(instr);
}


inline HInstruction* HBlock::Add(HInstruction* instr) {
  if (!ended_) instructions_.Push(instr);

  return instr;
}


inline HInstruction* HBlock::Goto(HInstruction::Type type, HBlock* target) {
  HInstruction* res = Add(type);

  if (!ended_) {
    AddSuccessor(target);
    ended_ = true;
  }

  return res;
}


inline HInstruction* HBlock::Branch(HInstruction::Type type, HBlock* t, HBlock* f) {
  HInstruction* res = Add(type);

  if (!ended_) {
    AddSuccessor(t);
    AddSuccessor(f);
    ended_ = true;
  }

  return res;
}


inline HInstruction* HBlock::Return(HInstruction::Type type) {
  HInstruction* res = Add(type);
  if (!ended_) ended_ = true;
  return res;
}


inline HBlock* HGen::Join(HBlock* b1, HBlock* b2) {
  HBlock* join = CreateBlock();

  b1->Goto(HInstruction::kGoto, join);
  b2->Goto(HInstruction::kGoto, join);

  return join;
}


inline HInstruction* HGen::Assign(ScopeSlot* slot, HInstruction* value) {
  return current_block()->Assign(slot, value);
}


inline bool HBlock::IsEnded() {
  return ended_;
}


inline bool HBlock::IsEmpty() {
  return instructions_.length() == 0;
}


inline bool HBlock::IsLoop() {
  return loop_;
}


inline HPhi* HBlock::CreatePhi(ScopeSlot* slot) {
  HPhi* phi =  new HPhi(g_, this, slot);

  phis_.Push(phi);

  return phi;
}


inline HEnvironment* HBlock::env() {
  assert(env_ != NULL);
  return env_;
}


inline void HBlock::env(HEnvironment* env) {
  env_ = env;
}


inline void HBlock::Print(PrintBuffer* p) {
  p->Print(IsLoop() ? "# Block %d (loop)\n" : "# Block %d\n", id);

  HInstructionList::Item* head = instructions_.head();
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


inline HInstruction* HEnvironment::At(int i) {
  assert(i < stack_slots_);
  return instructions_[i];
}


inline void HEnvironment::Set(int i, HInstruction* value) {
  assert(i < stack_slots_);
  instructions_[i] = value;
}


inline HPhi* HEnvironment::PhiAt(int i) {
  assert(i < stack_slots_);
  return phis_[i];
}


inline void HEnvironment::SetPhi(int i, HPhi* phi) {
  assert(i < stack_slots_);
  phis_[i] = phi;
}


inline HInstruction* HEnvironment::At(ScopeSlot* slot) {
  return At(slot->index());
}


inline void HEnvironment::Set(ScopeSlot* slot, HInstruction* value) {
  Set(slot->index(), value);
}


inline HPhi* HEnvironment::PhiAt(ScopeSlot* slot) {
  return PhiAt(slot->index());
}


inline void HEnvironment::SetPhi(ScopeSlot* slot, HPhi* phi) {
  SetPhi(slot->index(), phi);
}


inline int HEnvironment::stack_slots() {
  return stack_slots_;
}


inline ScopeSlot* HEnvironment::logic_slot() {
  return logic_slot_;
}


inline HBlockList* BreakContinueInfo::continue_blocks() {
  return &continue_blocks_;
}

} // namespace hir
} // namespace internal
} // namespace candor

#endif // _SRC_HIR_INL_H_
