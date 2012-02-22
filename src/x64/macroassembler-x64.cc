#include "macroassembler-x64.h"
#include "stubs.h"

namespace dotlang {

Masm::Masm(Heap* heap) : result_(scratch),
                         slot_(new Operand(rax, 0)),
                         heap_(heap),
                         stubs_(new Stubs(this)),
                         align_(0) {
}


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


void Masm::AlignCode() {
  offset_ = RoundUp(offset_, 16);
  Grow();
}


Masm::Align::Align(Masm* masm) : masm_(masm), align_(masm->align_){
  if (align_ % 2 == 0) return;
  masm_->subq(rsp, (align_ % 2) * 8);
}


Masm::Align::~Align() {
  if (align_ % 2 == 0) return;
  masm_->addq(rsp, (align_ % 2) * 8);
}


void Masm::Allocate(Heap::HeapTag tag, uint32_t size, Register result) {
  if (!result.is(rax)) {
    ChangeAlign(1);
    push(rax);
  }

  ChangeAlign(2);
  Align a(this);

  push(Immediate(size));
  push(Immediate(tag));
  Call(stubs()->GetAllocateStub());

  ChangeAlign(-2);

  if (!result.is(rax)) {
    movq(result, rax);
    pop(rax);
    ChangeAlign(-1);
  }
}


void Masm::AllocateContext(uint32_t slots, Register result) {
  // We can use any registers here
  // because context allocation is performed in prelude
  // and nothing can be affected yet
  Allocate(Heap::kContext, sizeof(void*) * (slots + 3), result);

  // Move address of current context to first slot
  Operand qparent(result, 8);
  movq(qparent, rdi);
}


void Masm::AllocateNumber(int64_t value, Register scratch, Register result) {
  Allocate(Heap::kNumber, 16, result);

  Operand qvalue(result, 8);
  movq(scratch, Immediate(value));
  movq(qvalue, scratch);
}


void Masm::Call(Register fn) {
  push(rdi);
  movq(rdi, fn);

  Operand code(rdi, 16);
  callq(code);

  pop(rdi);
}


void Masm::Call(BaseStub* stub) {
  movq(scratch, Immediate(0));
  stub->Use(offset());

  callq(scratch);
}

} // namespace dotlang
