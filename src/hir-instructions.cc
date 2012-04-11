#include "hir-instructions.h"
#include "hir.h"

#include "lir.h" // LIROperand
#include "lir-inl.h" // GetSpill

namespace candor {
namespace internal {

#define HIR_ENUM_STUB_INSTRUCTIONS(V)\
    V(Call)\
    V(StoreProperty)\
    V(LoadProperty)\
    V(BinOp)\
    V(Typeof)\
    V(Sizeof)\
    V(Keysof)\
    V(Not)\
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
    V(LoadRoot)\
    V(LoadContext)\
    V(BranchBool)\
    HIR_ENUM_STUB_INSTRUCTIONS(V)

void HIRInstruction::Init(HIRBasicBlock* block) {
  block_ = block;
}


void HIRInstruction::Use(HIRValue* value) {
  value->uses()->Push(this);
}


void HIRInstruction::ReplaceVarUse(HIRValue* source, HIRValue* target) {
  ZoneList<HIRValue*>::Item* item = values()->head();
  bool found = false;
  while (item != NULL) {
    if (item->value() == source) {
      ZoneList<HIRValue*>::Item* next = item->next();
      values()->InsertBefore(item, target);
      values()->Remove(item);
      found = true;
      item = next;
      continue;
    }
    item = item->next();
  }

  // Nop's return value doesn't define anything
  // TODO: Use something better to represent use-def
  if (type() == HIRInstruction::kNop && GetResult() == source) {
    result_ = target;
    found = true;
  }

  if (found) {
    // Remove instr from source uses
    ZoneList<HIRInstruction*>::Item* instr = source->uses()->head();
    while (instr != NULL) {
      if (instr->value() == this) {
        ZoneList<HIRInstruction*>::Item* next = instr->next();
        source->uses()->Remove(instr);
        instr = next;
        continue;
      }
      instr = instr->next();
    }

    // Add instr to target uses
    target->uses()->Push(this);
  }
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

  p->Print("%d: ", id());

  if (GetResult() != NULL) {
    GetResult()->Print(p);
    p->Print(" = ");
  }

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
  p->Print("]\n");
}


void HIRParallelMove::AddMove(LIROperand* source, LIROperand* target) {
  // Skip nop moves
  if (source->is_equal(target)) return;

#ifndef NDEBUG
  // Target should not appear twice in raw list
  OperandList::Item* sitem = raw_sources()->head();
  OperandList::Item* titem = raw_targets()->head();
  for (; sitem != NULL; sitem = sitem->next(), titem = titem->next()) {
    if (titem->value()->is_equal(target)) {
      assert(0 && "Dublicate target in movement");
    }
  }
#endif // NDEBUG

  raw_sources()->Push(source);
  raw_targets()->Push(target);
}


void HIRParallelMove::Reorder(LIR* lir,
                              ZoneList<LIROperand*>::Item* source,
                              ZoneList<LIROperand*>::Item* target) {
  // Mark source/target pair as `being moved`
  source->value()->move_status(LIROperand::kBeingMoved);
  target->value()->move_status(LIROperand::kBeingMoved);

  // Detect successors
  OperandList::Item* sitem = raw_sources()->head();
  OperandList::Item* titem = raw_targets()->head();
  for (; sitem != NULL; sitem = sitem->next(), titem = titem->next()) {
    if (!sitem->value()->is_equal(target->value())) continue;

    switch (sitem->value()->move_status()) {
     case LIROperand::kToMove:
      // Just successor
      Reorder(lir, sitem, titem);
      break;
     case LIROperand::kBeingMoved:
      // Lazily create spill operand
      if (spill_ == NULL) {
        spill_ = lir->GetSpill();
        lir->InsertMoveSpill(this, this, spill_);
      }

      // scratch = target
      sources()->Push(sitem->value());
      targets()->Push(spill_);

      // And use scratch in this move
      sitem->value(spill_);
      break;
     case LIROperand::kMoved:
      // NOP
      break;
     default:
      UNEXPECTED
      break;
    }
  }

  // And put pair into resulting list
  sources()->Push(source->value());
  targets()->Push(target->value());

  // Finalize status
  source->value()->move_status(LIROperand::kMoved);
  target->value()->move_status(LIROperand::kMoved);
}


void HIRParallelMove::Reorder(LIR* lir) {
  OperandList::Item* sitem = raw_sources()->head();
  OperandList::Item* titem = raw_targets()->head();
  for (; sitem != NULL; sitem = sitem->next(), titem = titem->next()) {
    if (sitem->value()->move_status() != LIROperand::kToMove) continue;
    Reorder(lir, sitem, titem);
  }

  // Reset move_status and empty list
  LIROperand* op;
  while ((op = raw_sources()->Shift()) != NULL) {
    op->move_status(LIROperand::kToMove);
  }
  while ((op = raw_targets()->Shift()) != NULL) {
    op->move_status(LIROperand::kToMove);
  }
}


void HIRParallelMove::Reset() {
  LIROperand* op;
  while ((op = raw_sources()->Shift()) != NULL) {}
  while ((op = raw_targets()->Shift()) != NULL) {}
  while ((op = sources()->Shift()) != NULL) {}
  while ((op = targets()->Shift()) != NULL) {}
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


void HIREntry::AddArg(HIRValue* arg) {
  Use(arg);
  args()->Push(arg);
  values()->Push(arg);
}


void HIRCall::AddArg(HIRValue* arg) {
  Use(arg);
  args()->Push(arg);
  values()->Push(arg);
}

#undef HIR_ENUM_INSTRUCTONS
#undef HIR_ENUM_STUB_INSTRUCTIONS

} // namespace internal
} // namespace candor
