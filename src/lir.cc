#include "hir.h"
#include "hir-inl.h"
#include "lir.h"
#include "lir-inl.h"
#include "lir-instructions.h"
#include "lir-instructions-inl.h"
#include <string.h> // memset

namespace candor {
namespace internal {

LGen::LGen(HIRGen* hir) : hir_(hir),
                          instr_id_(0),
                          interval_id_(0),
                          virtual_index_(40),
                          current_block_(NULL),
                          current_instruction_(NULL) {
  FlattenBlocks();
  GenerateInstructions();
  ComputeLocalLiveSets();
  ComputeGlobalLiveSets();
  BuildIntervals();
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


void LGen::GenerateInstructions() {
  HIRBlockList::Item* head = blocks_.head();

  for (; head != NULL; head = head->next()) {
    HIRBlock* b = head->value();
    current_block_ = b;

    // Generate lir form of block
    new LBlock(b);

    HIRInstructionList::Item* ihead = b->instructions()->head();
    for (; ihead != NULL; ihead = ihead->next()) {
      current_instruction_ = ihead->value();
      VisitInstruction(ihead->value());
    }
  }
}

#define LGEN_VISIT_SWITCH(V) \
    case HIRInstruction::k##V: Visit##V(instr); break;

void LGen::VisitInstruction(HIRInstruction* instr) {
  switch (instr->type()) {
    HIR_INSTRUCTION_TYPES(LGEN_VISIT_SWITCH)
   default:
    UNEXPECTED
  }
}

#undef LGEN_VISIT_SWITCH

void LGen::ComputeLocalLiveSets() {
  HIRBlockList::Item* head = blocks_.head();

  for (; head != NULL; head = head->next()) {
    HIRBlock* b = head->value();
    LBlock* l = b->lir();

    LInstructionList::Item* ihead = b->linstructions()->head();
    for (; ihead != NULL; ihead = ihead->next()) {
      LInstruction* instr = ihead->value();

      // Inputs to live_gen
      for (int i = 0; i < instr->input_count(); i++) {
        LUse* input = instr->inputs[i];
        NumberKey* key = NumberKey::New(input->interval()->id);

        if (l->live_kill.Get(key) == NULL) {
          l->live_gen.Set(key, input);
        }
      }

      // Scratches to live_kill
      for (int i = 0; i < instr->scratch_count(); i++) {
        LUse* scratch = instr->scratches[i];
        l->live_kill.Set(NumberKey::New(scratch->interval()->id), scratch);
      }

      // Result to live_kill
      if (instr->result) {
        LUse* result = instr->result;
        l->live_kill.Set(NumberKey::New(result->interval()->id), result);
      }
    }
  }
}


void LGen::ComputeGlobalLiveSets() {
  bool change;
  LUseMap::Item* mitem;

  do {
    change = false;

    // Traverse blocks in reverse order
    HIRBlockList::Item* tail = blocks_.tail();
    for (; tail != NULL; tail = tail->prev()) {
      HIRBlock* b = tail->value();
      LBlock* l = b->lir();

      // Every successor's input adds to current's output
      for (int i = 0; i < b->succ_count(); i++) {
        mitem = b->SuccAt(i)->lir()->live_in.head();
        for (; mitem != NULL; mitem = mitem->next_scalar()) {
          if (l->live_out.Get(mitem->key()) == NULL) {
            l->live_out.Set(mitem->key(), mitem->value());
            change = true;
          }
        }
      }

      // Inputs are live_gen...
      mitem = l->live_gen.head();
      for (; mitem != NULL; mitem = mitem->next_scalar()) {
        if (l->live_in.Get(mitem->key()) == NULL) {
          l->live_in.Set(mitem->key(), mitem->value());
          change = true;
        }
      }

      // ...and everything in output that isn't killed by current block
      mitem = l->live_out.head();
      for (; mitem != NULL; mitem = mitem->next_scalar()) {
        if (l->live_in.Get(mitem->key()) == NULL &&
            l->live_kill.Get(mitem->key()) == NULL) {
          l->live_in.Set(mitem->key(), mitem->value());
          change = true;
        }
      }
    }

    // Loop while there're any changes
  } while (change);
}


void LGen::BuildIntervals() {
  // Traverse blocks in reverse order
  HIRBlockList::Item* tail = blocks_.tail();
  LUseMap::Item* mitem;
  for (; tail != NULL; tail = tail->prev()) {
    HIRBlock* b = tail->value();
    LBlock* l = b->lir();

    // Set block's start and end instruction ids
    l->start_id = b->linstructions()->head()->value()->id;
    l->end_id = b->linstructions()->tail()->value()->id;

    // Add full block range to intervals that live out of this block
    // (we'll shorten those range later if needed).
    mitem = l->live_out.head();
    for (; mitem != NULL; mitem = mitem->next_scalar()) {
      mitem->value()->interval()->AddRange(l->start_id, l->end_id);
    }

    // And instructions too
    LInstructionList::Item* itail = b->linstructions()->tail();
    for (; itail != NULL; itail = itail->prev()) {
      LInstruction* instr = itail->value();

      if (instr->HasCall()) {
        // XXX: Insert fixed interval for each physical register
      }

      if (instr->result) {
        LInterval* res = instr->result->interval();

        // Add [id, id+1) range, result isn't used anywhere except in the
        // instruction itself
        if (res->ranges() == 0) {
          res->AddRange(instr->id, instr->id + 1);
        } else {
          // Shorten first range
          res->ranges()->head()->value()->start(instr->id);
        }
      }

      // Scratches are live only inside instruction
      for (int i = 0; i < instr->scratch_count(); i++) {
        instr->scratches[i]->interval()->AddRange(instr->id, instr->id + 1);
      }

      // Inputs are initially live from block's start to instruction
      for (int i = 0; i < instr->input_count(); i++) {
        instr->inputs[i]->interval()->AddRange(l->start_id, instr->id);
      }
    }
  }
}


void LGen::Print(PrintBuffer* p) {
  HIRBlockList::Item* bhead = blocks_.head();

  PrintIntervals(p);

  for (; bhead != NULL; bhead = bhead->next()) {
    HIRBlock* b = bhead->value();
    b->lir()->PrintHeader(p);

    LInstructionList::Item* ihead = b->linstructions()->head();
    for (; ihead != NULL; ihead = ihead->next()) {
      ihead->value()->Print(p);
    }
  }
}


void LGen::PrintIntervals(PrintBuffer* p) {
  LIntervalList::Item* ihead = intervals_.head();
  for (; ihead != NULL; ihead = ihead->next()) {
    LInterval* interval = ihead->value();
    p->Print("%02d: ", interval->id);
    for (int i = 0; i < instr_id_; i++) {
      LUse* use = interval->UseAt(i);
      if (use == NULL) {
        if (interval->Covers(i)) {
          p->Print("_");
        } else {
          p->Print(".");
        }
      } else {
        switch (use->type()) {
         case LUse::kRegister: p->Print("R"); break;
         case LUse::kAny: p->Print("A"); break;
         default: UNEXPECTED
        }
      }
    }
    p->Print("\n");
  }

  p->Print("\n");
}


LInterval* LGen::ToFixed(HIRInstruction* instr, Register reg) {
  LInterval* res = CreateRegister(reg);

  Add(LInstruction::kMove)
      ->SetResult(res, LUse::kRegister)
      ->AddArg(instr, LUse::kAny);

  return res;
}


LInterval* LGen::FromFixed(Register reg, LInterval* interval) {
  LInterval* res = CreateRegister(reg);

  Add(LInstruction::kMove)
      ->SetResult(interval, LUse::kAny)
      ->AddArg(res, LUse::kRegister);

  return res;
}


LInterval* LGen::FromFixed(Register reg, HIRInstruction* instr) {
  LInterval* res = CreateRegister(reg);

  Add(LInstruction::kMove)
      ->SetResult(instr, LUse::kAny)
      ->AddArg(res, LUse::kRegister);

  return res;
}


LUse* LInterval::Use(LUse::Type type, LInstruction* instr) {
  LUse* use = new LUse(this, type, instr);

  uses_.InsertSorted<LUseShape>(use);

  return use;
}


LRange* LInterval::AddRange(int start, int end) {
  LRange* range = new LRange(this, start, end);

  ranges_.InsertSorted<LRangeShape>(range);

  return range;
}


bool LInterval::Covers(int pos) {
  LRangeList::Item* head = ranges_.head();
  for (; head != NULL; head = head->next()) {
    LRange* range = head->value();
    if (range->start() > pos) return false;
    if (range->end() > pos) return true;
  }

  return false;
}


LUse* LInterval::UseAt(int pos) {
  LUseList::Item* head = uses_.head();
  for (; head != NULL; head = head->next()) {
    LUse* use = head->value();
    if (use->instr()->id == pos) return use;
  }

  return NULL;
}


LRange::LRange(LInterval* op, int start, int end) : op_(op),
                                                   start_(start),
                                                   end_(end) {
}


int LRangeShape::Compare(LRange* a, LRange* b) {
  return a->start() > b->start() ? 1 : a->start() < b->start() ? -1 : 0;
}


int LUseShape::Compare(LUse* a, LUse* b) {
  return a->instr()->id > b->instr()->id ? 1 :
         a->instr()->id < b->instr()->id ? -1 : 0;
}


LBlock::LBlock(HIRBlock* hir) : start_id(-1),
                                end_id(-1),
                                hir_(hir) {
  hir->lir(this);
}

} // namespace internal
} // namespace candor
