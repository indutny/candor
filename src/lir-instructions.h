#ifndef _SRC_LIR_INSTRUCTIONS_H_
#define _SRC_LIR_INSTRUCTIONS_H_

#include "lir.h"
#include "hir-instructions.h"
#include "hir-instructions-inl.h"
#include "zone.h"
#include "utils.h"

namespace candor {
namespace internal {

// Forward declarations
class LInstruction;
class ScopeSlot;
typedef ZoneList<LInstruction*> LInstructionList;

#define LIR_INSTRUCTION_TYPES(V) \
    V(Nop) \
    V(Nil) \
    V(Move) \
    V(Entry) \
    V(Return) \
    V(Function) \
    V(LoadArg) \
    V(LoadContext) \
    V(StoreContext) \
    V(LoadProperty) \
    V(StoreProperty) \
    V(DeleteProperty) \
    V(If) \
    V(Literal) \
    V(Goto) \
    V(Not) \
    V(BinOp) \
    V(Typeof) \
    V(Sizeof) \
    V(Keysof) \
    V(Clone) \
    V(Call) \
    V(CollectGarbage) \
    V(GetStackTrace) \
    V(AllocateObject) \
    V(CloneObject) \
    V(AllocateArray) \
    V(Phi)

#define LIR_INSTRUCTION_ENUM(I) \
    k##I,

#define LIR_INSTRUCTION_TYPE_STR(I) \
    case k##I: res = #I; break;

class LInstruction : public ZoneObject {
 public:
  enum Type {
    LIR_INSTRUCTION_TYPES(LIR_INSTRUCTION_ENUM)
    kNone
  };

  LInstruction(Type type) : type_(type),
                            id(-1),
                            input_count_(0),
                            scratch_count_(0),
                            slot_(NULL),
                            hir_(NULL) {
    inputs[0] = NULL;
    inputs[1] = NULL;
    scratches[0] = NULL;
    scratches[1] = NULL;

    result = NULL;
  }

  inline LInstruction* AddArg(LInterval* arg, LUse::Type use_type) {
    assert(input_count_ < 2);
    inputs[input_count_++] = arg->Use(use_type, this);

    return this;
  }

  inline LInstruction* AddArg(LInstruction* arg, LUse::Type use_type) {
    assert(arg->result != NULL);
    return AddArg(arg->result->interval(), use_type);
  }

  inline LInstruction* AddArg(HIRInstruction* arg, LUse::Type use_type) {
    return AddArg(arg->lir(), use_type);
  }

  inline LInstruction* AddScratch(LInterval* scratch) {
    assert(scratch_count_ < 2);
    scratches[scratch_count_++] = scratch->Use(LUse::kRegister, this);

    return this;
  }

  inline LInstruction* SetResult(LInterval* res, LUse::Type use_type) {
    assert(result == NULL);
    result = res->Use(use_type, this);

    return this;
  }

  inline LInstruction* SetSlot(ScopeSlot* slot) {
    assert(slot_ == NULL);
    slot_ = slot;

    return this;
  }

  inline Type type() { return type_; }
  int id;

  static inline const char* TypeToStr(Type type) {
    const char* res = NULL;
    switch (type) {
     LIR_INSTRUCTION_TYPES(LIR_INSTRUCTION_TYPE_STR)
     default:
      UNEXPECTED
      break;
    }

    return res;
  }

  inline void Print(PrintBuffer* p) {
    p->Print("%d: ", id);

    if (result) {
      result->Print(p);
      p->Print(" = ");
    }

    p->Print("%s", TypeToStr(type()));

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

  int input_count() { return input_count_; }
  int result_count() { return result != NULL; }
  int scratch_count() { return scratch_count_; }

  inline HIRInstruction* hir() { return hir_; }
  inline void hir(HIRInstruction* hir) { hir_ = hir; }

  LUse* inputs[2];
  LUse* scratches[2];
  LUse* result;

 private:
  Type type_;
  int input_count_;
  int scratch_count_;

  ScopeSlot* slot_;
  HIRInstruction* hir_;
};

#undef LIR_INSTRUCTION_ENUM

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_INSTRUCTIONS_H_
