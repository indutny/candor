#ifndef _SRC_LIR_INSTRUCTIONS_X64_H_
#define _SRC_LIR_INSTRUCTIONS_X64_H_

#include "zone.h"

namespace candor {
namespace internal {

// Forward declarations
class Masm;
class HIRInstruction;

#define LIR_ENUM_INSTRUCTIONS(V)\
    V(ParallelMove)\
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

static const int kLIRRegisterCount = 11;

class LIRInstruction : public ZoneObject {
 public:
  enum Type {
    LIR_ENUM_INSTRUCTIONS(LIR_GEN_TYPE_ENUM)
    kNone
  };

  LIRInstruction() : hir_(NULL) {
  }

  virtual void Generate() = 0;

  virtual Type type() const = 0;

  inline HIRInstruction* generic_hir() { return hir_; }
  inline void hir(HIRInstruction* hir) { hir_ = hir; }

  inline Masm* masm() { return masm_; }
  inline void masm(Masm* masm) { masm_ = masm; }

  inline int id() { return id_; }
  inline void id(int id) { id_ = id; }

  virtual int inputs() const = 0;
  virtual int results() const = 0;
  virtual int temporary() const = 0;

 protected:
  HIRInstruction* hir_;
  Masm* masm_;

  int id_;
};

// I <- input registers count
// R <- result registers
// T <- scratch registers count
template <int I, int R, int T>
class LIRInstructionTemplate : public LIRInstruction {
 public:
  LIRInstructionTemplate() {
  }

  int inputs() const { return I; }
  int results() const { return R; }
  int temporary() const { return T; }
};

template <int I, int T>
class LIRControlInstructionTemplate : public LIRInstruction {
 public:
  LIRControlInstructionTemplate() {
  }

  int inputs() const { return I; }
  int results() const { return 0; }
  int temporary() const { return T; }
};


#define LIR_COMMON_METHODS(V)\
    Type type() const { return k##V; }\
    inline HIR##V* hir() { return reinterpret_cast<HIR##V*>(generic_hir()); }

class LIRParallelMove : public LIRInstructionTemplate<0, 0, 0> {
 public:
  void Generate();

  LIR_COMMON_METHODS(ParallelMove);
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

class LIRStoreLocal : public LIRInstructionTemplate<1, 0, 0> {
 public:
  void Generate();

  LIR_COMMON_METHODS(StoreLocal)
};

class LIRStoreContext : public LIRInstructionTemplate<1, 0, 0> {
 public:
  void Generate();

  LIR_COMMON_METHODS(StoreContext)
};

class LIRStoreProperty : public LIRInstructionTemplate<1, 0, 0> {
 public:
  void Generate();

  LIR_COMMON_METHODS(StoreProperty)
};

class LIRLoadRoot : public LIRInstructionTemplate<0, 1, 0> {
 public:
  void Generate();

  LIR_COMMON_METHODS(LoadRoot)
};

class LIRLoadLocal : public LIRInstructionTemplate<0, 1, 0> {
 public:
  void Generate();

  LIR_COMMON_METHODS(LoadLocal)
};

class LIRLoadContext : public LIRInstructionTemplate<0, 1, 0> {
 public:
  void Generate();

  LIR_COMMON_METHODS(LoadContext)
};

class LIRBranchBool : public LIRControlInstructionTemplate<1, 0> {
 public:
  void Generate();

  LIR_COMMON_METHODS(BranchBool)
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
