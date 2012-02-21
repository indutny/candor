#include "macroassembler-x64.h"

namespace dotlang {

void Masm::Allocate(Register result,
                    Register result_end,
                    uint32_t size,
                    Register scratch) {

  Label runtime_allocate, done;

  Immediate top(reinterpret_cast<uint64_t>(heap_->new_space()->top()));
  Immediate limit(reinterpret_cast<uint64_t>(heap_->new_space()->limit()));

  Operand scratch_op(scratch, 0);

  // Get pointer to current page's top
  movq(scratch, top);
  movq(result, scratch_op);
  movq(result_end, result);

  // Add object size to the top
  addq(result_end, Immediate(size));
  jmp(kCarry, &runtime_allocate);

  // Check if we exhausted buffer
  movq(scratch, limit);
  cmp(result_end, scratch_op);
  jmp(kLe, &done);

  // Invoke runtime allocation stub (and probably GC)
  bind(&runtime_allocate);
    

  // Voila result and result_end are pointers
  bind(&done);
}


void Masm::AllocateContext(uint32_t slots) {
  // We can use any registers here
  // because context allocation is performed in prelude
  // and nothing can be affected yet
  Allocate(rax, rbx, sizeof(void*) * (slots + 2), scratch);

  Operand qtag(rax, 0);
  Operand qparent(rax, 8);

  movq(qtag, Heap::kContext);
  movq(qparent, rsi);

  // Replace reference to context
  movq(rsi, rax);
}

} // namespace dotlang
