#include "hir-instructions.h"
#include "hir.h"
#include "hir-inl.h"

#include "lir.h" // LIROperand
#include "lir-inl.h" // GetSpill

namespace candor {
namespace internal {

#define HIR_ENUM_STUB_INSTRUCTIONS(V)\
    V(Call)\
    V(StoreProperty)\
    V(LoadProperty)\
    V(DeleteProperty)\
    V(BinOp)\
    V(Typeof)\
    V(Sizeof)\
    V(Keysof)\
    V(Not)\
    V(CloneObject)\
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
  MoveList::Item* item = raw_moves()->head();
  for (; item != NULL; item = item->next()) {
    MoveItem* move = item->value();
    if (move->target()->is_equal(target)) {
      assert(0 && "Dublicate target in movement");
    }
  }
#endif // NDEBUG

  raw_moves()->Push(new MoveItem(source, target));
}


void HIRParallelMove::Reorder(LIR* lir, MoveItem* move) {
  // Mark source/target pair as `being moved`
  move->move_status(kBeingMoved);

  // Detect successors
  MoveList::Item* item = raw_moves()->head();
  for (; item != NULL; item = item->next()) {
    if (move->source()->is_equal(move->target())) break;

    MoveItem* next = item->value();
    if (!next->source()->is_equal(move->target())) continue;

    switch (next->move_status()) {
     case kToMove:
      // Just successor
      Reorder(lir, next);
      break;
     case kBeingMoved:
      // scratch = target
      moves()->Push(new MoveItem(next->source(), lir->tmp_spill()));

      // And use scratch in this move
      next->source(lir->tmp_spill());
      break;
     case kMoved:
      // NOP
      break;
     default:
      UNEXPECTED
      break;
    }
  }

  // And put pair into resulting list
  moves()->Push(move);

  // Finalize status
  move->move_status(kMoved);
}


void HIRParallelMove::Reorder(LIR* lir) {
  MoveList::Item* item = raw_moves()->head();
  for (; item != NULL; item = item->next()) {
    if (item->value()->move_status() != kToMove) continue;
    Reorder(lir, item->value());
  }

  // Reset move_status and empty list
  MoveItem* move;
  while ((move = raw_moves()->Shift()) != NULL) {
    move->move_status(kToMove);
  }
}


void HIRParallelMove::Reset() {
  MoveItem* move;
  while ((move = raw_moves()->Shift()) != NULL) {}
  while ((move = moves()->Shift()) != NULL) {}
}


void HIRParallelMove::Print(char* buffer, uint32_t size) {
  PrintBuffer p(buffer, size);

  MoveList* m[2] = { raw_moves(), moves() };

  for (int i = 0; i < 2; i++) {
    MoveList::Item* item = m[i]->head();
    for (; item != NULL; item = item->next()) {
      MoveItem* move = item->value();

      move->source()->Print(&p);
      p.Print(" => ");
      move->target()->Print(&p);
      p.Print("\n");
    }
    if (i == 0) p.Print("----\n");
  }

  p.Finalize();
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
