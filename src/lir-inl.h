#ifndef _SRC_LIR_INL_H_
#define _SRC_LIR_INL_H_

#include "lir.h"
#include "lir-instructions.h"

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


inline LInstruction* LGen::Add(int type) {
  LInstruction* r = new LInstruction(static_cast<LInstruction::Type>(type));

  r->id = instr_id();
  instructions_.Push(r);
  current_block_->linstructions()->Push(r);

  return r;
}


inline LInstruction* LGen::Bind(int type) {
  LInstruction* r = Add(type);

  current_instruction_->lir(r);
  r->hir(current_instruction_);

  return r;
}


inline LInterval* LGen::ToFixed(HIRInstruction* instr, Register reg) {
  LInterval* res = CreateRegister(reg);

  Add(LInstruction::kMove)
      ->SetResult(res, LUse::kRegister)
      ->AddArg(instr, LUse::kAny);

  return res;
}


inline LInterval* LGen::FromFixed(Register reg, LInterval* interval) {
  LInterval* res = CreateRegister(reg);

  Add(LInstruction::kMove)
      ->SetResult(interval, LUse::kAny)
      ->AddArg(res, LUse::kRegister);

  return res;
}


inline LInterval* LGen::FromFixed(Register reg, HIRInstruction* instr) {
  LInterval* res = CreateRegister(reg);

  Add(LInstruction::kMove)
      ->AddArg(res, LUse::kRegister)
      ->AddArg(instr, LUse::kAny);

  return res;
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
  return CreateInterval(LInterval::kRegister, reg.code());
}


inline LInterval* LGen::CreateStackSlot(int index) {
  return CreateInterval(LInterval::kStackSlot, index);
}


inline void LGen::Print(PrintBuffer* p) {
  HIRBlockList::Item* bhead = blocks_.head();
  for (; bhead != NULL; bhead = bhead->next()) {
    HIRBlock* b = bhead->value();
    p->Print("# Block: %d\n", b->id);

    LBlock* l = b->lir();
    p->Print("# in: ");
    LUseMap::Item* mitem = l->live_in.head();
    for (; mitem != NULL; mitem = mitem->next_scalar()) {
      p->Print("%d", static_cast<int>(mitem->key()->value()));
      if (mitem->next_scalar() != NULL) p->Print(", ");
    }

    p->Print(", out: ");
    mitem = l->live_out.head();
    for (; mitem != NULL; mitem = mitem->next_scalar()) {
      p->Print("%d", static_cast<int>(mitem->key()->value()));
      if (mitem->next_scalar() != NULL) p->Print(", ");
    }
    p->Print("\n");

    LInstructionList::Item* ihead = b->linstructions()->head();
    for (; ihead != NULL; ihead = ihead->next()) {
      ihead->value()->Print(p);
    }
  }
}


inline void LGen::Print(char* out, int32_t size) {
  PrintBuffer p(out, size);
  Print(&p);
}


inline HIRBlock* LBlock::hir() {
  assert(hir_ != NULL);
  return hir_;
}


inline int LRange::start() {
  return start_;
}


inline int LRange::end() {
  return end_;
}


inline LInstruction* LUse::instr() {
  return instr_;
}


inline LInterval* LUse::interval() {
  return interval_;
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

  p->Print("%d:%d", index(), id);
}


inline int LInterval::index() {
  return index_;
}

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_INL_H_
