#ifndef _SRC_LIR_INSTRUCTIONS_X64_H_
#define _SRC_LIR_INSTRUCTIONS_X64_H_

#include "zone.h"

namespace candor {
namespace internal {

// Forward declarations
class Masm;
class HIRInstruction;
class LIROperand;
struct Register;
class Operand;

#define LIR_ENUM_INSTRUCTIONS(V)\
    V(Nop)\
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
    V(LoadProperty)\
    V(BranchBool)\
    V(BinOp)\
    V(Call)\
    V(Typeof)\
    V(Sizeof)\
    V(Keysof)\
    V(AllocateObject)\
    V(AllocateFunction)

#define LIR_GEN_FORWARD_HIR_DECL(V)\
    class HIR##V;

LIR_ENUM_INSTRUCTIONS(LIR_GEN_FORWARD_HIR_DECL)

#undef LIR_GEN_FORWARD_HIR_DECL

#define LIR_GEN_TYPE_ENUM(V)\
    k##V,

static const int kLIRRegisterCount = 10;

class LIRInstruction : public ZoneObject {
 public:
  enum Type {
    LIR_ENUM_INSTRUCTIONS(LIR_GEN_TYPE_ENUM)
    kNone
  };

  LIRInstruction() : hir_(NULL) {
    // Nullify all inputs/outputs/scratches
    inputs[0] = NULL;
    inputs[1] = NULL;
    inputs[2] = NULL;

    scratches[0] = NULL;
    scratches[1] = NULL;

    result = NULL;
  }

  virtual void Generate() = 0;

  virtual Type type() const = 0;

  // Short-hand for converting operand to register
  inline Register ToRegister(LIROperand* op);
  inline Operand& ToOperand(LIROperand* op);
  inline LIROperand* ToLIROperand(Register reg);

  inline HIRInstruction* generic_hir() { return hir_; }
  inline void hir(HIRInstruction* hir) { hir_ = hir; }

  inline Masm* masm() { return masm_; }
  inline void masm(Masm* masm) { masm_ = masm; }

  inline int id() { return id_; }
  inline void id(int id) { id_ = id; }

  virtual int input_count() const = 0;
  virtual int result_count() const = 0;
  virtual int scratch_count() const = 0;

  LIROperand* inputs[3];
  LIROperand* scratches[2];
  LIROperand* result;

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

  int input_count() const { return I; }
  int result_count() const { return R; }
  int scratch_count() const { return T; }
};

template <int I, int T>
class LIRControlInstructionTemplate : public LIRInstruction {
 public:
  LIRControlInstructionTemplate() {
  }

  int input_count() const { return I; }
  int result_count() const { return 0; }
  int scratch_count() const { return T; }
};


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

class LIRStoreProperty : public LIRInstructionTemplate<2, 1, 0> {
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

class LIRLoadProperty : public LIRInstructionTemplate<2, 1, 0> {
 public:
  LIRLoadProperty();

  void Generate();

  LIR_COMMON_METHODS(LoadProperty)
};

class LIRBranchBool : public LIRControlInstructionTemplate<1, 0> {
 public:
  void Generate();

  LIR_COMMON_METHODS(BranchBool)
};

class LIRBinOp : public LIRInstructionTemplate<1, 2, 0> {
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
