#ifndef _SRC_LIR_INSTRUCTIONS_X64_H_
#define _SRC_LIR_INSTRUCTIONS_X64_H_

#include "zone.h"

namespace candor {
namespace internal {

// Forward declarations
class Masm;
class HIRInstruction;

#define LIR_ENUM_INSTRUCTIONS(V)\
    V(Entry)\
    V(Return)\
    V(Goto)\
    V(StoreLocal)\
    V(StoreContext)\
    V(StoreProperty)\
    V(LoadRoot)\
    V(LoadLocal)\
    V(LoadContext)\
    V(BranchBool)\
    V(AllocateObject)

#define LIR_GEN_TYPE_ENUM(V)\
    k##V,

class LIRInstruction : public ZoneObject {
 public:
  enum Type {
    LIR_ENUM_INSTRUCTIONS(LIR_GEN_TYPE_ENUM)
    kNone
  };

  LIRInstruction() : hir_(NULL) {
  }

  virtual void Generate() = 0;

  inline HIRInstruction* hir() { return hir_; }
  inline void hir(HIRInstruction* hir) { hir_ = hir; }

  inline Masm* masm() { return masm_; }
  inline void masm(Masm* masm) { masm_ = masm; }

 private:
  HIRInstruction* hir_;
  Masm* masm_;
};

// I <- input registers count
// R <- result registers
// T <- scratch registers count
template <int I, int R, int T>
class LIRInstructionTemplate : public LIRInstruction {
 public:
};

template <int I, int T>
class LIRControlInstructionTemplate : public LIRInstruction {
 public:
};

class LIREntry : public LIRInstructionTemplate<0, 0, 0> {
 public:
  void Generate();
};

class LIRReturn : public LIRInstructionTemplate<1, 0, 0> {
 public:
  void Generate();
};

class LIRGoto : public LIRInstructionTemplate<0, 0, 0> {
 public:
  void Generate();
};

class LIRStoreLocal : public LIRInstructionTemplate<1, 0, 0> {
 public:
  void Generate();
};

class LIRStoreContext : public LIRInstructionTemplate<1, 0, 0> {
 public:
  void Generate();
};

class LIRStoreProperty : public LIRInstructionTemplate<1, 0, 0> {
 public:
  void Generate();
};

class LIRLoadRoot : public LIRInstructionTemplate<0, 1, 0> {
 public:
  void Generate();
};

class LIRLoadLocal : public LIRInstructionTemplate<0, 1, 0> {
 public:
  void Generate();
};

class LIRLoadContext : public LIRInstructionTemplate<0, 1, 0> {
 public:
  void Generate();
};

class LIRBranchBool : public LIRControlInstructionTemplate<1, 0> {
 public:
  void Generate();
};

class LIRAllocateObject : public LIRInstructionTemplate<0, 1, 0> {
 public:
  void Generate();
};

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_INSTRUCTIONS_X64_H_
