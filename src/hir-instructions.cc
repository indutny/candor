#include "hir-instructions.h"
#include "hir.h"

#include "lir.h" // LIROperand

namespace candor {
namespace internal {

#define HIR_ENUM_STUB_INSTRUCTIONS(V)\
    V(Call)\
    V(AllocateContext)\
    V(AllocateFunction)\
    V(AllocateObject)

#define HIR_ENUM_INSTRUCTIONS(V)\
    V(None)\
    V(Nop)\
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
    HIR_ENUM_STUB_INSTRUCTIONS(V)

void HIRInstruction::Init(HIRBasicBlock* block) {
  block_ = block;
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
  if (type() != kGoto) {
    ZoneList<HIRValue*>::Item* item = values()->head();
    if (item != NULL) p->Print(" ");
    while (item != NULL) {
      item->value()->Print(p);
      item = item->next();
      if (item != NULL) p->Print(" ");
    }
  }
  p->Print("]");
}


void HIRParallelMove::AddMove(LIROperand* source, LIROperand* target) {
  raw_sources_.Push(source);
  raw_targets_.Push(target);
}


void HIRParallelMove::Reorder(LIROperand* source, LIROperand* target) {
  // Mark source/target pair as `being moved`
  source->being_moved(true);
  target->being_moved(true);

  // Detect successors
  OperandList::Item* sitem = raw_sources_.head();
  OperandList::Item* titem = raw_targets_.head();
  for (; sitem != NULL; sitem = sitem->next(), titem = titem->next()) {
    if (!sitem->value()->is_equal(target)) continue;

    if (sitem->value()->being_moved()) {
      // Loop detected - create `scratch` operand
      LIROperand* scratch = new LIROperand(LIROperand::kSpill,
                                           static_cast<off_t>(-1));

      // scratch = target
      sources()->Push(target);
      targets()->Push(scratch);

      // And use scratch in this move
      sitem->value(scratch);
    } else {
      // Just successor
      Reorder(sitem->value(), titem->value());
    }
  }

  // Reset marks
  source->being_moved(false);
  target->being_moved(false);

  // And put pair into resulting list
  sources()->Push(source);
  targets()->Push(target);
}


void HIRParallelMove::Reorder() {
  LIROperand* source;
  LIROperand* target;
  while (raw_sources_.length() > 0) {
    // Get source/target pair from work list
    source = raw_sources_.Shift();
    target = raw_targets_.Shift();

    Reorder(source, target);
  }
}


void HIRBranchBase::Init(HIRBasicBlock* block) {
  HIRInstruction::Init(block);

  block->AddSuccessor(left());
  block->AddSuccessor(right());
}


void HIRStubCall::Init(HIRBasicBlock* block) {
  HIRInstruction::Init(block);

  HIRValue* result = block->hir()->CreateValue(block);
  SetResult(result);
}


void HIRCall::AddArg(HIRValue* arg) {
  Use(arg);
  args()->Push(arg);
}

#undef HIR_ENUM_INSTRUCTONS
#undef HIR_ENUM_STUB_INSTRUCTIONS

} // namespace internal
} // namespace candor
