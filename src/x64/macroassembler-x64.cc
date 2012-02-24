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


void Masm::Allocate(Heap::HeapTag tag,
                    Register size_reg,
                    uint32_t size,
                    Register result) {
  if (!result.is(rax)) {
    ChangeAlign(1);
    push(rax);
  }

  ChangeAlign(2);
  {
    Align a(this);

    // Add tag size
    if (size_reg.is(reg_nil)) {
      movq(rax, Immediate(size + sizeof(void*)));
    } else {
      movq(rax, size_reg);
      addq(rax, Immediate(sizeof(void*)));
    }
    push(rax);
    movq(rax, Immediate(tag));
    push(rax);
    Call(stubs()->GetAllocateStub());
    // Stub will unwind stack
  }
  ChangeAlign(-2);

  if (!result.is(rax)) {
    movq(result, rax);
    pop(rax);
    ChangeAlign(-1);
  }
}


void Masm::AllocateContext(uint32_t slots) {
  ChangeAlign(1);
  push(rax);

  // parent + number of slots + slots
  Allocate(Heap::kTagContext, reg_nil, sizeof(void*) * (slots + 2), rax);

  // Move address of current context to first slot
  Operand qparent(rax, 8);
  movq(qparent, rdi);

  // Save number of slots
  Operand qslots(rax, 16);
  movq(qslots, Immediate(slots));

  // Replace current context
  // (It'll be restored by caller)
  movq(rdi, rax);
  pop(rax);
  ChangeAlign(-1);
}


void Masm::AllocateFunction(Register addr, Register result) {
  // context + code
  Allocate(Heap::kTagFunction, reg_nil, sizeof(void*) * 2, result);

  // Move address of current context to first slot
  Operand qparent(result, 8);
  Operand qaddr(result, 16);
  movq(qparent, rdi);
  movq(qaddr, addr);
}


void Masm::AllocateNumber(Register value, Register result) {
  ChangeAlign(1);
  push(value);

  // int64_t value
  Allocate(Heap::kTagNumber, reg_nil, 8, result);

  pop(value);
  ChangeAlign(-1);
  Operand qvalue(result, 8);
  movq(qvalue, scratch);
}


void Masm::AllocateObjectLiteral(Register size, Register result) {
  // mask (= (size - 1) << 3) + space = ( size * 2 * 8 bytes : key/value )
  movq(result, size);
  shl(result, Immediate(4));
  inc(result);

  Allocate(Heap::kTagObject, result, 0, result);

  Operand qmask(result, 8);

  // Compute mask and store it
  push(size);
  dec(size);
  shl(size, Immediate(3));
  movq(qmask, size);
  pop(size);

  // Fill space's key slots (first half) with nil
  push(rcx);
  push(result);

  // Skip tag and mask
  addq(result, Immediate(2 * sizeof(void*)));

  // Calculate end of range
  movq(rcx, result);
  addq(rcx, size);

  Label entry(this), loop(this);
  jmp(&entry);
  bind(&loop);

  // Fill
  Operand op(result, 0);
  movq(op, Immediate(0));

  // Move
  addq(result, Immediate(8));

  bind(&entry);

  // And loop
  cmp(result, rcx);
  jmp(kLt, &loop);

  pop(result);
  pop(rcx);
}


void Masm::FillStackSlots(uint32_t slots) {
  if (slots == 0) return;

  movq(rax, Immediate(Heap::kTagNil));
  for (uint32_t i = 0; i < slots; i ++) {
    Operand slot(rbp, -sizeof(void*) * (i + 1));
    movq(slot, rax);
  }
}


void Masm::IsNil(Register reference, Label* is_nil) {
  cmp(reference, Immediate(Heap::kTagNil));
  jmp(kEq, is_nil);
}


void Masm::IsHeapObject(Heap::HeapTag tag,
                        Register reference,
                        Label* mismatch) {
  Operand qtag(reference, 0);
  cmp(qtag, Immediate(tag));
  jmp(kNe, mismatch);
}


void Masm::UnboxNumber(Register number) {
  Operand qvalue(number, 8);
  movq(number, qvalue);
}


void Masm::StoreRootStack() {
  Immediate root_stack(reinterpret_cast<uint64_t>(heap()->root_stack()));
  Operand scratch_op(scratch, 0);
  movq(scratch, root_stack);
  movq(scratch_op, rbp);
}


void Masm::Throw(Heap::Error error) {
  Immediate pending_exception(
      reinterpret_cast<uint64_t>(heap()->pending_exception()));
  Immediate root_stack(reinterpret_cast<uint64_t>(heap()->root_stack()));

  // Set pending exception
  Operand scratch_op(scratch, 0);
  movq(scratch, pending_exception);
  movq(rax, Immediate(error));
  movq(scratch_op, rax);

  // Unwind stack to the top handler
  movq(scratch, root_stack);
  movq(rsp, scratch_op);

  // Return NULL
  movq(rax, 0);

  // Leave to C++ land
  pop(rbx);
  pop(rbp);
  ret(0);
}


void Masm::Call(Register fn, uint32_t args) {
  Operand context_slot(fn, 8);
  Operand code_slot(fn, 16);
  movq(rdi, context_slot);
  movq(rsi, Immediate(args));

  callq(code_slot);
}


void Masm::Call(BaseStub* stub) {
  movq(scratch, Immediate(0));
  stub->Use(offset());

  callq(scratch);
}

} // namespace dotlang
