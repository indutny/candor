/**
 * Copyright (c) 2012, Fedor Indutny.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _SRC_MARCOASSEMBLER_H_
#define _SRC_MARCOASSEMBLER_H_

#include "assembler.h"
#include "assembler-inl.h"
#include "ast.h"  // AstNode
#include "code-space.h"  // CodeSpace
#include "heap.h"  // Heap::HeapTag and etc
#include "heap-inl.h"

namespace candor {
namespace internal {

// Forward declaration
class BaseStub;
class LUse;

class Masm : public Assembler {
 public:
  explicit Masm(CodeSpace* space);

  // Save/restore all valuable register
  void Pushad();
  void Popad(Register preserve);

  class Align {
   public:
    explicit Align(Masm* masm);
    ~Align();
   private:
    Masm* masm_;
    int32_t align_;
  };

  class Spill {
   public:
    explicit Spill(Masm* masm);
    Spill(Masm* masm, Register src);
    ~Spill();

    void SpillReg(Register src);

    void Unspill(Register dst);
    void Unspill();
    Operand* GetOperand();

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
  void FinalizeSpills();

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

  // Generic move, LIR augmentation
  void Move(LUse* dst, LUse* src);
  void Move(LUse* dst, Register src);
  void Move(LUse* dst, const Operand& src);
  void Move(LUse* dst, Immediate src);

  // Sets correct environment and calls function
  void Call(Register addr);
  void Call(const Operand& addr);
  void Call(char* stub);
  void CallFunction(Register fn);
  void ProbeCPU();

  enum BinOpUsage {
    kIntegral,
    kDouble
  };

  inline void Push(Register src);
  inline void Pop(Register src);
  inline void PreservePop(Register src, Register preserve);
  inline void TagNumber(Register src);
  inline void Untag(Register src);
  inline Operand& SpillToOperand(int index);
  inline Condition BinOpToCondition(BinOp::BinOpType type, BinOpUsage usage);
  inline void SpillSlot(uint32_t index, Operand* op);

  inline Heap* heap() { return space_->heap(); }
  inline Stubs* stubs() { return space_->stubs(); }
  inline CodeSpace* space() { return space_; }

  inline void stack_slots(uint32_t stack_slots) {
    spill_offset_ = (1 + stack_slots) * HValue::kPointerSize;
  }

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

class AbsoluteAddress {
 public:
  AbsoluteAddress() : ip_(-1), r_(NULL) {}

  void Target(Masm* masm, int offset);
  void Use(Masm* masm, int offset);
  void NotifyGC();

 private:
  void Commit(Masm* masm);

  int ip_;
  RelocationInfo* r_;
};

}  // namespace internal
}  // namespace candor

#endif  // _SRC_MARCOASSEMBLER_H_
