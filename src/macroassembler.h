#ifndef _SRC_MARCOASSEMBLER_H_
#define _SRC_MARCOASSEMBLER_H_

#include "assembler.h"
#include "ast.h" // AstNode
#include "code-space.h" // CodeSpace
#include "heap.h" // Heap::HeapTag and etc
#include "heap-inl.h"

namespace candor {
namespace internal {

// Forward declaration
class BaseStub;
class LIROperand;

class Masm : public Assembler {
 public:
  Masm(CodeSpace* space);

  // Save/restore all valuable register
  void Pushad();
  void Popad(Register preserve);

  class Align {
   public:
    Align(Masm* masm);
    ~Align();
   private:
    Masm* masm_;
    int32_t align_;
  };

  class Spill {
   public:
    Spill(Masm* masm);
    Spill(Masm* masm, Register src);
    ~Spill();

    void SpillReg(Register src);

    void Unspill(Register dst);
    void Unspill();

    inline bool is_empty() { return src_.is(reg_nil); }

    inline Masm* masm() { return masm_; }
    inline int32_t index() { return index_; }

   private:
    Masm* masm_;

    Register src_;
    int32_t index_;
  };

  // Allocate slots for spills
  void AllocateSpills();
  void FinalizeSpills(int spills);

  // Skip some bytes to make code aligned
  void AlignCode();

  // Alignment helpers
  inline void ChangeAlign(int32_t slots) { align_ += slots; }

  // Allocate some space in heap's new space current page
  // Jmp to runtime_allocate label on exhaust or fail
  void Allocate(Heap::HeapTag tag,
                Register size_reg,
                uint32_t size,
                Register result);

  // Allocate context and function
  void AllocateContext(uint32_t slots);

  // Allocate heap numbers
  void AllocateNumber(DoubleRegister value, Register result);

  // Allocate object&map
  void AllocateObjectLiteral(Heap::HeapTag tag,
                             Register tag_reg,
                             Register size,
                             Register result);

  // VarArg
  void AllocateVarArgSlots(Spill* vararg, Register argc);

  // Fills memory segment with immediate value
  void Fill(Register start, Register end, Immediate value);

  // Fill stack slots with nil
  void FillStackSlots();

  // Generate enter/exit frame sequences
  void EnterFramePrologue();
  void EnterFrameEpilogue();
  void ExitFramePrologue();
  void ExitFrameEpilogue();

  // Compute string's hash
  void StringHash(Register str, Register result);

  // Perform garbage collection if needed (heap flag is set)
  void CheckGC();

  void IsNil(Register reference, Label* not_nil, Label* is_nil);
  void IsUnboxed(Register reference, Label* not_unboxed, Label* unboxed);

  // Checks if object has specific type
  void IsHeapObject(Heap::HeapTag tag,
                    Register reference,
                    Label* mismatch,
                    Label* match);
  void IsTrue(Register reference, Label* is_false, Label* is_true);
  void IsDenseArray(Register reference, Label* non_dense, Label* dense);

  // Sets correct environment and calls function
  void Call(Register addr);
  void Call(Operand& addr);
  void Call(char* stub);
  void CallFunction(Register fn);
  void ProbeCPU();

  enum BinOpUsage {
    kIntegral,
    kDouble
  };

  // Routines for working with operands
  inline void Push(LIROperand* src);
  inline void Mov(Register dst, LIROperand* src);
  inline void Mov(LIROperand* dst, Register src);
  inline void Mov(LIROperand* dst, LIROperand* src);

  inline void Push(Register src);
  inline void Pop(Register src);
  inline void PreservePop(Register src, Register preserve);
  inline void TagNumber(Register src);
  inline void Untag(Register src);
  inline Operand& SpillToOperand(int index);
  inline Condition BinOpToCondition(BinOp::BinOpType type, BinOpUsage usage);
  inline void SpillSlot(uint32_t index, Operand& op);

  inline Heap* heap() { return space_->heap(); }
  inline Stubs* stubs() { return space_->stubs(); }

 protected:
  CodeSpace* space_;

  int32_t align_;

  RelocationInfo* spill_reloc_;
  uint32_t spill_offset_;
  int32_t spill_index_;
  int32_t spills_;

  // Temporary operand
  Operand spill_operand_;

  friend class Align;
};

} // namespace internal
} // namespace candor

#endif // _SRC_MARCOASSEMBLER_H_
