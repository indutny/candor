#ifndef _SRC_X64_MARCOASSEMBLER_H_
#define _SRC_X64_MARCOASSEMBLER_H_

#include "assembler-x64.h"
#include "ast.h" // AstNode
#include "heap.h" // HeapValue

namespace dotlang {

// Forward declaration
class BaseStub;
class Stubs;

class Masm : public Assembler {
 public:
  Masm(Heap* heap);

  // Save/restore all valuable register
  void Pushad();
  void Popad();

  class Align {
   public:
    Align(Masm* masm);
    ~Align();
   private:
    Masm* masm_;
    int32_t align_;
  };

  // Skip some bytes to make code aligned
  void AlignCode();

  // Alignment helpers
  inline void ChangeAlign(int32_t slots) { align_ += slots; }

  // Allocate some space in heap's new space current page
  // Jmp to runtime_allocate label on exhaust or fail
  void Allocate(Heap::HeapTag tag, uint32_t size, Register result);

  // Allocate function context
  void AllocateContext(uint32_t slots, Register result);

  // Allocate heap number (XXX: should unbox numbers if possible)
  void AllocateNumber(Register value, Register result);

  // Sets correct environment and calls function
  void Call(Register fn);
  void Call(BaseStub* stub);

  inline void Save(Register src) {
    if (!result().is(src)) {
      push(src);
      ChangeAlign(1);
    }
  }

  inline void Restore(Register src) {
    if (!result().is(src)) {
      pop(src);
      ChangeAlign(-1);
    }
  }

  inline void Result(Register src) {
    if (!result().is(src)) movq(result(), src);
  }

  // See VisitForSlot and VisitForValue in fullgen for disambiguation
  inline Register result() { return result_; }
  inline Operand* slot() { return slot_; }
  inline Heap* heap() { return heap_; }
  inline Stubs* stubs() { return stubs_; }

  Register result_;
  Operand* slot_;

 protected:
  Heap* heap_;
  Stubs* stubs_;

  int32_t align_;

  friend class Align;
};

} // namespace dotlang

#endif // _SRC_X64_MARCOASSEMBLER_H_
