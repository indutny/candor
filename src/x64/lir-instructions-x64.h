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

#define LIR_GEN_FORWARD_HIR_DECL(V)\
    class HIR##V;

LIR_ENUM_INSTRUCTIONS(LIR_GEN_FORWARD_HIR_DECL)

#undef LIR_GEN_FORWARD_HIR_DECL

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

  inline HIRInstruction* generic_hir() { return hir_; }
  inline void hir(HIRInstruction* hir) { hir_ = hir; }

  inline Masm* masm() { return masm_; }
  inline void masm(Masm* masm) { masm_ = masm; }

 protected:
  HIRInstruction* hir_;
  Masm* masm_;

  int inputs_;
  int outputs_;
  int temporary_;
};

// I <- input registers count
// R <- result registers
// T <- scratch registers count
template <int I, int R, int T>
class LIRInstructionTemplate : public LIRInstruction {
 public:
  LIRInstructionTemplate() {
    inputs_ = I;
    outputs_ = R;
    temporary_ = T;
  }
};

template <int I, int T>
class LIRControlInstructionTemplate : public LIRInstruction {
 public:
  LIRControlInstructionTemplate() {
    inputs_ = I;
    temporary_ = T;
  }
};


#define HIR_GETTER(V)\
    inline HIR##V* hir() { return reinterpret_cast<HIR##V*>(generic_hir()); }

class LIREntry : public LIRInstructionTemplate<0, 0, 0> {
 public:
  void Generate();

  HIR_GETTER(Entry)
};

class LIRReturn : public LIRInstructionTemplate<1, 0, 0> {
 public:
  void Generate();

  HIR_GETTER(Return)
};

class LIRGoto : public LIRInstructionTemplate<0, 0, 0> {
 public:
  void Generate();

  HIR_GETTER(Goto)
};

class LIRStoreLocal : public LIRInstructionTemplate<1, 0, 0> {
 public:
  void Generate();

  HIR_GETTER(StoreLocal)
};

class LIRStoreContext : public LIRInstructionTemplate<1, 0, 0> {
 public:
  void Generate();

  HIR_GETTER(StoreContext)
};

class LIRStoreProperty : public LIRInstructionTemplate<1, 0, 0> {
 public:
  void Generate();

  HIR_GETTER(StoreProperty)
};

class LIRLoadRoot : public LIRInstructionTemplate<0, 1, 0> {
 public:
  void Generate();

  HIR_GETTER(LoadRoot)
};

class LIRLoadLocal : public LIRInstructionTemplate<0, 1, 0> {
 public:
  void Generate();

  HIR_GETTER(LoadLocal)
};

class LIRLoadContext : public LIRInstructionTemplate<0, 1, 0> {
 public:
  void Generate();

  HIR_GETTER(LoadContext)
};

class LIRBranchBool : public LIRControlInstructionTemplate<1, 0> {
 public:
  void Generate();

  HIR_GETTER(BranchBool)
};

class LIRAllocateObject : public LIRInstructionTemplate<0, 1, 0> {
 public:
  void Generate();

  HIR_GETTER(AllocateObject)
};


#undef HIR_GETTER

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_INSTRUCTIONS_X64_H_
