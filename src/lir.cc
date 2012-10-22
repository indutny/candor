#include "hir.h"
#include "hir-inl.h"
#include "lir.h"
#include "lir-inl.h"
#include <string.h> // memset

namespace candor {
namespace internal {

LGen::LGen(HIRGen* hir) : hir_(hir), instr_id_(0) {
  FlattenBlocks();
  ComputeLiveness();
}


void LGen::FlattenBlocks() {
  int* visits = reinterpret_cast<int*>(Zone::current()->Allocate(
      sizeof(*visits) * hir_->blocks()->length()));
  memset(visits, 0, sizeof(*visits) * hir_->blocks()->length());

  // Flatten blocks in a linear structure
  HIRBlockList work_queue;

  // Enqueue roots
  HIRBlockList::Item* head = hir_->roots()->head();
  for (; head != NULL; head = head->next()) {
    work_queue.Push(head->value());
  }

  while (work_queue.length() > 0) {
    HIRBlock* b = work_queue.Shift();

    visits[b->id]++;
    if (b->pred_count() == 0) {
      // Root block
    } else if (b->IsLoop()) {
      // Loop start
      if (visits[b->id] != 1) continue;
    } else if (visits[b->id] != b->pred_count()) {
      // Regular block
      continue;
    }

    blocks_.Push(b);

    for (int i = b->succ_count() - 1; i >= 0; i--) {
      work_queue.Unshift(b->SuccAt(i));
    }
  }
}


void LGen::ComputeLiveness() {
  // Compute kill/gen first
  HIRBlockList::Item* head = blocks_.head();
  for (; head != NULL; head = head->next()) {
    HIRBlock* b = head->value();

    HIRInstructionList::Item* ihead = b->instructions()->head();
    for (; ihead != NULL; ihead = ihead->next()) {
      HIRInstruction* i = ihead->value();

      b->live_gen()->Set(NumberKey::New(i->id), i);

      HIRInstructionList::Item* ahead = i->args()->head();
      for (; ahead != NULL; ahead = ahead->next()) {
        HIRInstruction* arg = ahead->value();
        b->live_kill()->Set(NumberKey::New(arg->id), arg);
      }
    }
  }
}


void LGen::VisitBlock(HIRBlock* block) {
  
}


LOperand::LOperand(int index, Type type) : index_(index), type_(type) {
}


LUse* LOperand::Use(LUse::Type type, LInstruction* instr) {
  LUse* use = new LUse(this, type, instr);

  uses_.InsertSorted<LUseShape>(use);

  return use;
}


LRange* LOperand::AddRange(int start, int end) {
  LRange* range = new LRange(this, start, end);

  ranges_.InsertSorted<LRangeShape>(range);

  return range;
}


LUnallocated::LUnallocated(int index) : LOperand(index, kUnallocated) {
}


LRegister::LRegister(int index) : LOperand(index, kRegister) {
}


LStackSlot::LStackSlot(int index) : LOperand(index, kStackSlot) {
}


LRange::LRange(LOperand* op, int start, int end) : op_(op),
                                                   start_(start),
                                                   end_(end) {
}


int LRangeShape::Compare(LRange* a, LRange* b) {
  return a->start() > b->start() ? 1 : a->start() < b->start() ? -1 : 0;
}


LUse::LUse(LOperand* op, Type type, LInstruction* instr) : op_(op),
                                                           type_(type),
                                                           instr_(instr) {
}


int LUseShape::Compare(LUse* a, LUse* b) {
  return a->instr()->id > b->instr()->id ? 1 :
         a->instr()->id < b->instr()->id ? -1 : 0;
}

} // namespace internal
} // namespace candor
