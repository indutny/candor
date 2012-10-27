#ifndef _SRC_LIR_INL_H_
#define _SRC_LIR_INL_H_

#include "lir.h"
#include "lir-instructions.h"
#include "macroassembler.h"

namespace candor {
namespace internal {

inline int LGen::instr_id() {
  int r = instr_id_;

  instr_id_ += 2;

  return r;
}


inline int LGen::interval_id() {
  return interval_id_++;
}


inline int LGen::virtual_index() {
  return virtual_index_++;
}


inline LInstruction* LGen::Add(LInstruction* instr) {
  instr->id = instr_id();
  instr->block(current_block_);
  instructions_.Push(instr);
  current_block_->instructions()->Push(instr);

  return instr;
}


inline LInstruction* LGen::Bind(LInstruction* instr) {
  LInstruction* r = Add(instr);

  current_instruction_->lir(r);
  r->hir(current_instruction_);

  return r;
}


inline LInterval* LGen::CreateInterval(LInterval::Type type, int index) {
  LInterval* res = new LInterval(type, index);
  res->id = interval_id();
  intervals_.Push(res);
  return res;
}


inline LInterval* LGen::CreateVirtual() {
  return CreateInterval(LInterval::kVirtual, virtual_index());
}


inline LInterval* LGen::CreateRegister(Register reg) {
  return CreateInterval(LInterval::kRegister, IndexByRegister(reg));
}


inline LInterval* LGen::CreateStackSlot(int index) {
  return CreateInterval(LInterval::kStackSlot, index);
}


inline LBlock* LGen::IsBlockStart(int pos) {
  HIRBlockList::Item* bhead = blocks_.head();
  for (; bhead != NULL; bhead = bhead->next()) {
    LBlock* block = bhead->value()->lir();
    if (block->start_id == pos) return block;

    // Optimization
    if (block->start_id > pos) return NULL;
  }

  return NULL;
}


inline void LGen::Print(char* out, int32_t size) {
  PrintBuffer p(out, size);
  Print(&p);
}


inline HIRBlock* LBlock::hir() {
  assert(hir_ != NULL);
  return hir_;
}


inline ZoneList<LInstruction*>* LBlock::instructions() {
  return &instructions_;
}


inline void LBlock::PrintHeader(PrintBuffer* p) {
  p->Print("# Block %d\n", hir()->id);

  if (live_in.head() != NULL || live_out.head() != NULL) {
    p->Print("# in: ");
    LUseMap::Item* mitem = live_in.head();
    for (; mitem != NULL; mitem = mitem->next_scalar()) {
      p->Print("%d", static_cast<int>(mitem->key()->value()));
      if (mitem->next_scalar() != NULL) p->Print(", ");
    }

    p->Print(", out: ");
    mitem = live_out.head();
    for (; mitem != NULL; mitem = mitem->next_scalar()) {
      p->Print("%d", static_cast<int>(mitem->key()->value()));
      if (mitem->next_scalar() != NULL) p->Print(", ");
    }
    p->Print("\n");
  }
}


inline LInterval* LRange::interval() {
  return interval_;
}


inline void LRange::interval(LInterval* interval) {
  interval_ = interval;
}


inline int LRange::start() {
  return start_;
}


inline void LRange::start(int start) {
  start_ = start;
}


inline int LRange::end() {
  return end_;
}


inline LInstruction* LUse::instr() {
  return instr_;
}


inline LUse::Type LUse::type() {
  return type_;
}


inline LInterval* LUse::interval() {
  return interval_;
}


inline void LUse::interval(LInterval* interval) {
  interval_ = interval;
}


inline void LUse::Print(PrintBuffer* p) {
  if (type_ == kRegister) p->Print("@");
  interval()->Print(p);
}


inline bool LInterval::is_virtual() {
  return type_ == kVirtual;
}


inline bool LInterval::is_register() {
  return type_ == kRegister;
}


inline bool LInterval::is_stackslot() {
  return type_ == kStackSlot;
}


inline void LInterval::Print(PrintBuffer* p) {
  switch (type_) {
   case kVirtual: p->Print("v"); break;
   case kRegister: p->Print("r"); break;
   case kStackSlot: p->Print("s"); break;
   default: UNEXPECTED
  }

  p->Print("%d", id);
  if (type_ == kRegister) {
    p->Print(":%s", RegisterNameByIndex(index()));
  } else {
    p->Print(":%d", index());
  }
}


inline void LInterval::Allocate(int reg) {
  assert(!IsFixed());
  type_ = kRegister;
  index_ = reg;
}


inline void LInterval::Spill(int slot) {
  assert(!IsFixed());
  type_ = kStackSlot;
  index_ = slot;
}


inline void LInterval::MarkFixed() {
  fixed_ = true;
}


inline bool LInterval::IsFixed() {
  return fixed_;
}


inline int LInterval::index() {
  return index_;
}


inline LRangeList* LInterval::ranges() {
  return &ranges_;
}


inline LUseList* LInterval::uses() {
  return &uses_;
}


inline LUse* LInterval::first_use() {
  assert(uses_.length() > 0);
  return uses_.head()->value();
}


inline LUse* LInterval::last_use() {
  assert(uses_.length() > 0);
  return uses_.tail()->value();
}


inline LInterval* LInterval::split_parent() {
  return split_parent_;
}


inline void LInterval::split_parent(LInterval* split_parent) {
  split_parent_ = split_parent;
}


inline LIntervalList* LInterval::split_children() {
  return &split_children_;
}


inline int LInterval::start() {
  assert(ranges()->length() > 0);
  return ranges()->head()->value()->start();
}


inline int LInterval::end() {
  assert(ranges()->length() > 0);
  return ranges()->tail()->value()->end();
}

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_INL_H_
