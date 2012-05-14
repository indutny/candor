#ifndef _SRC_LIR_INSTRUCTIONS_H_
#define _SRC_LIR_INSTRUCTIONS_H_

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
    V(LoadContext)\
    V(LoadProperty)\
    V(DeleteProperty)\
    V(BranchBool)\
    V(BinOp)\
    V(Call)\
    V(Typeof)\
    V(Sizeof)\
    V(Keysof)\
    V(Not)\
    V(CloneObject)\
    V(CollectGarbage)\
    V(GetStackTrace)\
    V(AllocateObject)\
    V(AllocateFunction)

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

  inline LIR* lir() { return lir_; }
  inline void lir(LIR* lir) { lir_ = lir; }

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
  LIR* lir_;
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

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_INSTRUCTIONS_H_
