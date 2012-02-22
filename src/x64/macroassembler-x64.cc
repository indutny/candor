#include "macroassembler-x64.h"
#include "runtime.h"

namespace dotlang {

void Masm::Pushad() {
  // 10 registers to save (10 * 8 = 16 * 5, so stack should be aligned)
  push(rax);
  push(rcx);
  push(rdx);
  push(rsi);
  push(rdi);
  push(r8);
  push(r9);
  push(r10);
  push(r11);

  // Last one just for alignment
  push(r15);
}


void Masm::Popad() {
  pop(r15);
  pop(r11);
  pop(r10);
  pop(r9);
  pop(r8);
  pop(rdi);
  pop(rsi);
  pop(rdx);
  pop(rcx);
  pop(rax);
}


Masm::Align::Align(Masm* masm) : masm_(masm), align_(masm->align_){
  if (align_ % 2 == 0) return;
  masm_->subq(rsp, align_ * 8);
}


Masm::Align::~Align() {
  if (align_ % 2 == 0) return;
  masm_->addq(rsp, align_ * 8);
}


void Masm::Allocate(Register result,
                    Register result_end,
                    Heap::HeapTag tag,
                    uint32_t size,
                    Register scratch) {

  Label runtime_allocate(this), done(this);

  Immediate heapref(reinterpret_cast<uint64_t>(heap()));
  Immediate top(reinterpret_cast<uint64_t>(heap()->new_space()->top()));
  Immediate limit(reinterpret_cast<uint64_t>(heap()->new_space()->limit()));

  Operand scratch_op(scratch, 0);

  // Get pointer to current page's top
  // (new_space()->top() is a pointer to space's property
  // which is a pointer to page's top pointer
  // that's why we are dereferencing it here twice
  movq(scratch, top);
  movq(scratch, scratch_op);
  movq(result, scratch_op);
  movq(result_end, result);

  // Add object size to the top
  addq(result_end, Immediate(size));
  jmp(kCarry, &runtime_allocate);

  // Check if we exhausted buffer
  movq(scratch, limit);
  movq(scratch, scratch_op);
  cmp(result_end, scratch_op);
  jmp(kLe, &done);

  // Invoke runtime allocation stub (and probably GC)
  bind(&runtime_allocate);

  RuntimeAllocateCallback allocate = &RuntimeAllocate;

  movq(rdi, heapref);
  movq(rsi, Immediate(size));
  movq(scratch, Immediate(*reinterpret_cast<uint64_t*>(&allocate)));
  {
    Align a(this);
    Pushad();
    callq(scratch);
    Popad();
  }

  // Voila result and result_end are pointers
  bind(&done);

  // Set tag
  Operand qtag(result, 0);
  movq(qtag, tag);
}


void Masm::AllocateContext(Register result,
                           Register result_end,
                           Register scratch,
                           uint32_t slots) {
  // We can use any registers here
  // because context allocation is performed in prelude
  // and nothing can be affected yet
  Allocate(result,
           result_end,
           Heap::kContext,
           sizeof(void*) * (slots + 3),
           scratch);

  // Move address of current context to first slot
  Operand qparent(result, 8);
  movq(qparent, rsi);
}


void Masm::AllocateNumber(Register result,
                          Register result_end,
                          Register scratch,
                          int64_t value) {
  Allocate(result, result_end, Heap::kNumber, 16, scratch);

  Operand qvalue(result, 8);
  movq(scratch, Immediate(value));
  movq(qvalue, scratch);
}

} // namespace dotlang
