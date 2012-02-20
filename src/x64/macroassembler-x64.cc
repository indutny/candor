#include "macroassembler-x64.h"

namespace dotlang {

void Masm::Allocate(Register result,
                    Register result_end,
                    uint32_t size,
                    Register scratch1,
                    Label* runtime_allocate) {
  Immediate top(reinterpret_cast<uint64_t>(heap_->new_space()->top()));
  Immediate limit(reinterpret_cast<uint64_t>(heap_->new_space()->limit()));

  Operand scratch_op(scratch, 0);

  movq(scratch, top);
  movq(result, scratch_op);
  movq(result_end, result);

  // Add object size to the top
  addq(result_end, Immediate(size));
  jmp(kCarry, runtime_allocate);

  // Check if we exhausted buffer
  movq(scratch, limit);
  cmp(result_end, scratch_op);
  jmp(kGt, runtime_allocate);
}


void Masm::AllocateContext(uint32_t slots) {
  // We can use any registers here
  // because context allocation is performed in prelude
  // and nothing can be affected yet
  Label runtime_alloc, end;
  Allocate(rax, rbx, sizeof(void*) * (slots + 2), scratch, &runtime_alloc);

  Operand qtag(rax, 0);
  Operand qparent(rax, 8);

  movq(qtag, Heap::kContext);
  movq(qparent, rsi);
  jmp(&end);

  bind(&runtime_alloc);
  // TODO: implement me
  emitb(0xcc);

  bind(&end);

  // Replace reference to context
  movq(rsi, rax);
}

} // namespace dotlang
