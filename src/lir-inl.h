#ifndef _SRC_LIR_INL_H_
#define _SRC_LIR_INL_H_

#include "hir.h"

#if CANDOR_ARCH_x64
#include "x64/lir-instructions-x64.h"
#elif CANDOR_ARCH_ia32
#include "ia32/lir-instructions-ia32.h"
#endif

#include "macroassembler.h"

#include <sys/types.h> // off_t

namespace candor {
namespace internal {

inline int HIRValueEndShape::Compare(HIRValue* l, HIRValue* r) {
  // Normal order (by end)
  return l->live_range()->end - r->live_range()->end;
}


inline void LIROperand::Print(PrintBuffer* p) {
  if (is_register()) {
    p->Print("%%%s", RegisterNameByIndex(value()));
  } else if (is_spill()) {
    p->Print("[%llu]", value());
  } else {
    p->Print("%llu", value());
  }
}


inline void LIR::Release(LIROperand* operand) {
  if (operand->is_register()) {
    registers()->Release(operand->value());
  } else if (operand->is_spill()) {
    spills()->Release(operand->value());
  }
}

inline void LIR::ChangeOperand(HIRInstruction* hinstr,
                               HIRValue* value,
                               LIROperand* operand) {
  if (value->operand() != NULL &&
      !value->operand()->is_equal(operand)) {
    Release(value->operand());
  }
  value->operand(operand);
}

inline LIRInstruction* LIR::Cast(HIRInstruction* instr) {
#define LIR_GEN_CAST(V)\
  case HIRInstruction::k##V:\
    result = new LIR##V();\
    break;

  LIRInstruction* result = NULL;
  switch (instr->type()) {
   LIR_ENUM_INSTRUCTIONS(LIR_GEN_CAST)
   default: UNEXPECTED break;
  }

  // Initialize instruction
  result->hir(instr);
  result->id(instr->id());
  result->lir(this);

  return result;
#undef LIR_GEN_CAST
}


inline void LIR::AddInstruction(LIRInstruction* instr) {
  if (last_instruction_ != NULL) {
    last_instruction_->next(instr);
    instr->prev(last_instruction_);
  } else {
    first_instruction_ = instr;
  }

  last_instruction_ = instr;

  // Store active values' operands into `operands` list
  HIRValueList::Item* item = active_values()->head();
  for (; item != NULL; item = item->next()) {
    if (item->value()->operand() == NULL) continue;
    item->value()->operand()->hir(item->value());
    instr->AddOperand(item->value()->operand());
  }
}


inline bool LIR::IsInUse(LIROperand* operand) {
  return (operand->is_register() && !registers()->Has(operand->value())) ||
         (operand->is_spill() && !spills()->Has(operand->value()));
}

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_INL_H_
