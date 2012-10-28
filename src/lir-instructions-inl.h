#ifndef _SRC_LIR_INSTRUCTIONS_INL_H_
#define _SRC_LIR_INSTRUCTIONS_INL_H_

#include "lir.h"

namespace candor {
namespace internal {

inline LInstruction* LInstruction::AddArg(LInterval* arg, LUse::Type use_type) {
  assert(arg != NULL);
  assert(input_count_ < 2);
  inputs[input_count_++] = arg->Use(use_type, this);

  return this;
}


inline LInstruction* LInstruction::AddArg(LInstruction* arg,
                                          LUse::Type use_type) {
  assert(arg != NULL);
  assert(arg->result != NULL);
  return AddArg(arg->propagated_->interval(), use_type);
}


inline LInstruction* LInstruction::AddArg(HIRInstruction* arg,
                                          LUse::Type use_type) {
  assert(arg != NULL);
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
  propagated_ = result;

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


inline LInstruction* LInstruction::Propagate(LUse* res) {
  propagated_ = res;

  return this;
}


inline LInstruction* LInstruction::Propagate(HIRInstruction* res) {
  assert(res->lir()->propagated_ != NULL);
  return Propagate(res->lir()->propagated_);
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

inline void LGap::Add(LInterval* src, LInterval* dst) {
  unhandled_pairs_.Push(new Pair(src, dst));
}


inline void LControlInstruction::AddTarget(LLabel* target) {
  assert(target_count_ < 2);
  targets_[target_count_++] = target;
}


inline LLabel* LControlInstruction::TargetAt(int i) {
  assert(i < target_count_);
  return targets_[i];
}


inline LControlInstruction* LControlInstruction::Cast(LInstruction* instr) {
  assert(instr->type() == kGoto ||
         instr->type() == kBranch);
  return reinterpret_cast<LControlInstruction*>(instr);
}

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_INSTRUCTIONS_INL_H_
