#ifndef _SRC_LIR_INSTRUCTIONS_INL_H_
#define _SRC_LIR_INSTRUCTIONS_INL_H_

#include "lir.h"

namespace candor {
namespace internal {

inline LInstruction* LInstruction::AddArg(LInterval* arg, LUse::Type use_type) {
  assert(input_count_ < 2);
  inputs[input_count_++] = arg->Use(use_type, this);

  return this;
}


inline LInstruction* LInstruction::AddArg(LInstruction* arg,
                                          LUse::Type use_type) {
  assert(arg->result != NULL);
  return AddArg(arg->result->interval(), use_type);
}


inline LInstruction* LInstruction::AddArg(HIRInstruction* arg,
                                          LUse::Type use_type) {
  return AddArg(arg->lir(), use_type);
}


inline LInstruction* LInstruction::AddScratch(LInterval* scratch) {
  assert(scratch_count_ < 2);
  scratches[scratch_count_++] = scratch->Use(LUse::kRegister, this);

  return this;
}


inline LInstruction* LInstruction::SetResult(LInterval* res,
                                             LUse::Type use_type) {
  assert(result == NULL);
  result = res->Use(use_type, this);

  return this;
}


inline LInstruction* LInstruction::SetResult(LInstruction* res,
                                             LUse::Type use_type) {
  assert(res->result != NULL);
  return SetResult(res->result->interval(), use_type);
}


inline LInstruction* LInstruction::SetResult(HIRInstruction* res,
                                             LUse::Type use_type) {
  return SetResult(res->lir(), use_type);
}


inline LInstruction* LInstruction::SetSlot(ScopeSlot* slot) {
  assert(slot_ == NULL);
  slot_ = slot;

  return this;
}

#define LIR_INSTRUCTION_TYPE_STR(I) \
    case LInstruction::k##I: res = #I; break;

inline const char* LInstruction::TypeToStr(LInstruction::Type type) {
  const char* res = NULL;
  switch (type) {
   LIR_INSTRUCTION_TYPES(LIR_INSTRUCTION_TYPE_STR)
   default:
    UNEXPECTED
    break;
  }

  return res;
}

#undef LIR_INSTRUCTION_TYPE_STR

inline void LInstruction::Print(PrintBuffer* p) {
  p->Print("%d: ", id);

  if (result) {
    result->Print(p);
    p->Print(" = ");
  }

  p->Print("%s", TypeToStr(type()));
  if (type() == kLiteral && hir()->ast() != NULL) {
    p->Print("[");
    p->PrintValue(hir()->ast()->value(), hir()->ast()->length());
    p->Print("]");
  }

  for (int i = 0; i < input_count(); i++) {
    if (i == 0) p->Print(" ");
    inputs[i]->Print(p);
    if (i + 1 < input_count()) p->Print(", ");
  }

  if (scratch_count()) {
    p->Print(" # scratches: ");
    for (int i = 0; i < scratch_count(); i++) {
      scratches[i]->Print(p);
      if (i + 1 < scratch_count()) p->Print(", ");
    }
  }

  p->Print("\n");
}

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_INSTRUCTIONS_INL_H_
