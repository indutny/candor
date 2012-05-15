#ifndef _SRC_LIR_INSTRUCTIONS_X64_H_
#define _SRC_LIR_INSTRUCTIONS_X64_H_

#include "lir-instructions.h"
#include "lir-instructions-inl.h"
#include "zone.h"

namespace candor {
namespace internal {

static const int kLIRRegisterCount = 10;

#define LIR_COMMON_METHODS(V)\
    Type type() const { return k##V; }\
    inline HIR##V* hir() { return reinterpret_cast<HIR##V*>(generic_hir()); }

class LIRParallelMove : public LIRInstructionTemplate<0, 0, 0> {
 public:
  void Generate();

  LIR_COMMON_METHODS(ParallelMove);
};

class LIRNop : public LIRInstructionTemplate<0, 0, 0> {
 public:
  void Generate();

  LIR_COMMON_METHODS(Nop)
};

class LIREntry : public LIRInstructionTemplate<0, 0, 0> {
 public:
  void Generate();

  LIR_COMMON_METHODS(Entry)
};

class LIRReturn : public LIRInstructionTemplate<1, 0, 0> {
 public:
  void Generate();

  LIR_COMMON_METHODS(Return)
};

class LIRGoto : public LIRInstructionTemplate<0, 0, 0> {
 public:
  void Generate();

  LIR_COMMON_METHODS(Goto)
};

class LIRStoreLocal : public LIRInstructionTemplate<1, 1, 0> {
 public:
  void Generate();

  LIR_COMMON_METHODS(StoreLocal)
};

class LIRStoreContext : public LIRInstructionTemplate<1, 1, 1> {
 public:
  void Generate();

  LIR_COMMON_METHODS(StoreContext)
};

class LIRStoreProperty : public LIRInstructionTemplate<3, 0, 1> {
 public:
  LIRStoreProperty();

  void Generate();

  LIR_COMMON_METHODS(StoreProperty)
};

class LIRLoadRoot : public LIRInstructionTemplate<0, 1, 0> {
 public:
  void Generate();

  LIR_COMMON_METHODS(LoadRoot)
};

class LIRLoadContext : public LIRInstructionTemplate<0, 1, 0> {
 public:
  void Generate();

  LIR_COMMON_METHODS(LoadContext)
};

class LIRLoadProperty : public LIRInstructionTemplate<2, 1, 1> {
 public:
  LIRLoadProperty();

  void Generate();

  LIR_COMMON_METHODS(LoadProperty)
};

class LIRDeleteProperty : public LIRInstructionTemplate<2, 1, 0> {
 public:
  LIRDeleteProperty();

  void Generate();

  LIR_COMMON_METHODS(DeleteProperty)
};

class LIRBranchBool : public LIRControlInstructionTemplate<1, 0> {
 public:
  LIRBranchBool();

  void Generate();

  LIR_COMMON_METHODS(BranchBool)
};

class LIRBinOp : public LIRInstructionTemplate<2, 1, 0> {
 public:
  LIRBinOp();

  void Generate();

  LIR_COMMON_METHODS(BinOp)
};

class LIRCall : public LIRInstructionTemplate<1, 1, 0> {
 public:
  void Generate();

  LIR_COMMON_METHODS(Call)
};

class LIRTypeof : public LIRInstructionTemplate<1, 1, 0> {
 public:
  LIRTypeof();

  void Generate();

  LIR_COMMON_METHODS(Typeof)
};

class LIRSizeof : public LIRInstructionTemplate<1, 1, 0> {
 public:
  LIRSizeof();

  void Generate();

  LIR_COMMON_METHODS(Sizeof)
};

class LIRKeysof : public LIRInstructionTemplate<1, 1, 0> {
 public:
  LIRKeysof();

  void Generate();

  LIR_COMMON_METHODS(Keysof)
};

class LIRNot : public LIRInstructionTemplate<1, 1, 0> {
 public:
  LIRNot();

  void Generate();

  LIR_COMMON_METHODS(Not)
};

class LIRCloneObject : public LIRInstructionTemplate<1, 1, 0> {
 public:
  void Generate();

  LIR_COMMON_METHODS(CloneObject)
};

class LIRCollectGarbage : public LIRInstructionTemplate<0, 1, 0> {
 public:
  void Generate();

  LIR_COMMON_METHODS(CollectGarbage)
};

class LIRGetStackTrace : public LIRInstructionTemplate<0, 1, 0> {
 public:
  void Generate();

  LIR_COMMON_METHODS(GetStackTrace)
};

class LIRAllocateFunction : public LIRInstructionTemplate<0, 1, 1> {
 public:
  void Generate();

  LIR_COMMON_METHODS(AllocateFunction)
};

class LIRAllocateObject : public LIRInstructionTemplate<0, 1, 0> {
 public:
  void Generate();

  LIR_COMMON_METHODS(AllocateObject)
};


#undef LIR_COMMON_METHODS

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_INSTRUCTIONS_X64_H_
