#include "hir-instructions.h"
#include "hir.h"

#include "lir.h" // LIROperand

namespace candor {
namespace internal {

#define HIR_ENUM_INSTRUCTIONS(V)\
  V(None)\
  V(ParallelMove)\
  V(Entry)\
  V(Return)\
  V(Goto)\
  V(StoreLocal)\
  V(StoreContext)\
  V(StoreProperty)\
  V(LoadRoot)\
  V(LoadLocal)\
  V(LoadContext)\
  V(BranchBool)\
  V(AllocateContext)\
  V(AllocateObject)

void HIRInstruction::Init(HIRBasicBlock* block, int id) {
  block_ = block;
  id_ = id;
}


void HIRInstruction::Use(HIRValue* value) {
  value->uses()->Push(this);
}


void HIRInstruction::Print(PrintBuffer* p) {
  const char* str;

#define HIR_GEN_SWITCH_CASE(V)\
  case k##V: str = #V; break;

  switch (type()) {
   HIR_ENUM_INSTRUCTIONS(HIR_GEN_SWITCH_CASE)
   default: str = NULL; break;
  }

#undef HIR_GEN_SWITCH_CASE

  p->Print("[%s", str);
  List<HIRValue*, ZoneObject>::Item* item = values()->head();
  if (item != NULL) p->Print(" ");
  while (item != NULL) {
    item->value()->Print(p);
    item = item->next();
    if (item != NULL) p->Print(" ");
  }
  p->Print("]");
}


HIRParallelMove::HIRParallelMove(HIRInstruction* instr, InsertionType type)
    : HIRInstruction(kParallelMove) {
  Init(instr->block(), type == kBefore ? instr->id() - 1 : instr->id() + 1);

  // Insert `move` into instruction's linked-list
  if (type == kBefore) {
    next(instr);
    prev(instr->prev());

    if (instr->prev() != NULL) instr->prev()->next(this);
    instr->prev(this);
  } else if (type == kAfter) {
    prev(instr);
    next(instr->next());

    if (instr->next() != NULL) instr->next()->prev(this);
    instr->next(this);
  } else {
    UNEXPECTED
  }
}


void HIRParallelMove::AddMove(LIROperand* source, LIROperand* target) {
  sources()->Push(source);
  targets()->Push(target);
}


void HIRBranchBase::Init(HIRBasicBlock* block, int id) {
  block->AddSuccessor(left());
  block->AddSuccessor(right());
}


void HIRStubCall::Init(HIRBasicBlock* block, int id) {
  SetResult(new HIRValue(block));
}

#undef HIR_ENUM_INSTRUCTONS

} // namespace internal
} // namespace candor
