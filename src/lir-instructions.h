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
                            has_call_(NULL),
                            slot_(NULL),
                            hir_(NULL),
                            propagated_(NULL) {
    inputs[0] = NULL;
    inputs[1] = NULL;
    scratches[0] = NULL;
    scratches[1] = NULL;

    result = NULL;
  }

  inline LInstruction* AddArg(LInterval* arg, LUse::Type use_type);
  inline LInstruction* AddArg(LInstruction* arg, LUse::Type use_type);
  inline LInstruction* AddArg(HIRInstruction* arg, LUse::Type use_type);

  inline LInstruction* AddScratch(LInterval* scratch);

  inline LInstruction* SetResult(LInterval* res, LUse::Type use_type);
  inline LInstruction* SetResult(LInstruction* res, LUse::Type use_type);
  inline LInstruction* SetResult(HIRInstruction* res, LUse::Type use_type);
  inline LInstruction* Propagate(LUse* res);
  inline LInstruction* Propagate(HIRInstruction* res);

  inline LInstruction* SetSlot(ScopeSlot* slot);

  inline LInstruction* MarkHasCall() { has_call_ = true; return this; }
  inline bool HasCall() { return has_call_; }

  inline Type type() { return type_; }
  int id;

  static inline const char* TypeToStr(Type type);

  inline void Print(PrintBuffer* p);

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
  bool has_call_;

  ScopeSlot* slot_;
  HIRInstruction* hir_;
  LUse* propagated_;
};

#undef LIR_INSTRUCTION_ENUM

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_INSTRUCTIONS_H_
