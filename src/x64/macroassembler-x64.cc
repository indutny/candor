#include "macroassembler-x64.h"
#include "macroassembler-x64-inl.h"
#include "stubs.h"
#include "utils.h" // ComputeHash

#include <stdlib.h> // NULL

namespace dotlang {

Masm::Masm(Heap* heap) : result_(rax),
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
  push(r12);

  // Last one just for alignment
  push(r15);
}


void Masm::Popad(Register preserve) {
  PreservePop(r15, preserve);
  PreservePop(r12, preserve);
  PreservePop(r10, preserve);
  PreservePop(r9, preserve);
  PreservePop(r8, preserve);
  PreservePop(rdi, preserve);
  PreservePop(rsi, preserve);
  PreservePop(rdx, preserve);
  PreservePop(rcx, preserve);
  PreservePop(rax, preserve);
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
    Push(rax);
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
    movq(rax, Immediate(TagNumber(tag)));
    push(rax);

    // Push context
    push(rsp);
    Call(stubs()->GetAllocateStub());
    // Stub will unwind stack
  }
  ChangeAlign(-2);

  if (!result.is(rax)) {
    movq(result, rax);
    Pop(rax);
  }
}


void Masm::AllocateContext(uint32_t slots) {
  Push(rax);

  // parent + number of slots + slots
  Allocate(Heap::kTagContext, reg_nil, sizeof(void*) * (slots + 2), rax);

  // Move address of current context to first slot
  Operand qparent(rax, 8);
  movq(qparent, rdi);

  // Save number of slots
  Operand qslots(rax, 16);
  movq(qslots, Immediate(slots));

  // Clear context
  for (uint32_t i = 0; i < slots; i++) {
    Operand qslot(rax, 24 + i * sizeof(void*));
    movq(qslot, Immediate(0));
  }

  // Replace current context
  // (It'll be restored by caller)
  movq(rdi, rax);
  Pop(rax);
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
  assert(0 && "Not implemented yet");
}


void Masm::AllocateBoolean(Register value, Register result) {
  // Value is often a scratch register, so store it before doing a stub call
  Push(value);

  Allocate(Heap::kTagBoolean, reg_nil, 1, result);

  Pop(value);

  Operand qvalue(result, 8);
  movb(qvalue, value);
}


void Masm::AllocateString(const char* value,
                          uint32_t length,
                          Register result) {
  // hash(8) + length(8)
  Allocate(Heap::kTagString, reg_nil, 16 + length, result);

  Operand qhash(result, 8);
  Operand qlength(result, 16);

  movq(qhash, Immediate(ComputeHash(value, length)));
  movq(qlength, Immediate(length));

  // Copy the value into (inlined)

  // By words first
  uint32_t i;
  for (i = 0; i < length - (length % 4); i += 4) {
    Operand lpos(result, 24 + i);
    movl(lpos, Immediate(*reinterpret_cast<const uint32_t*>(value + i)));
  }

  // And by bytes for last chars
  for (; i < length; i++) {
    Operand bpos(result, 24 + i);
    movb(bpos, Immediate(value[i]));
  }
}


void Masm::AllocateObjectLiteral(Register size, Register result) {
  // mask + map
  Allocate(Heap::kTagObject, reg_nil, 16, result);

  Operand qmask(result, 8);
  Operand qmap(result, 16);

  // Set mask
  movq(scratch, size);

  // mask (= (size - 1) << 3)
  dec(scratch);
  shl(scratch, Immediate(3));
  movq(qmask, scratch);

  // Create map
  PushTagged(size);

  // keys + values
  shl(size, Immediate(4));
  // + size
  addq(size, Immediate(8));
  Allocate(Heap::kTagMap, size, 0, scratch);
  movq(qmap, scratch);

  PopTagged(size);

  PushTagged(size);
  Push(result);
  movq(result, scratch);

  // Save map size for GC
  Operand qmapsize(result, 8);
  movq(qmapsize, size);

  // Fill map with nil
  shl(size, Immediate(4));
  addq(result, Immediate(16));
  addq(size, result);
  Fill(result, size, Immediate(Heap::kTagNil));
  Pop(result);
  PopTagged(size);
}


void Masm::Fill(Register start, Register end, Immediate value) {
  Push(start);
  movq(scratch, value);

  Label entry(this), loop(this);
  jmp(&entry);
  bind(&loop);

  // Fill
  Operand op(start, 0);
  movq(op, scratch);

  // Move
  addq(start, Immediate(8));

  bind(&entry);

  // And loop
  cmpq(start, end);
  jmp(kLt, &loop);

  Pop(start);
}


void Masm::FillStackSlots(uint32_t slots) {
  if (slots == 0) return;

  movq(scratch, Immediate(Heap::kTagNil));
  for (uint32_t i = 0; i < slots; i ++) {
    Operand slot(rbp, -sizeof(void*) * (i + 1));
    movq(slot, scratch);
  }
}


void Masm::IsNil(Register reference, Label* not_nil, Label* is_nil) {
  cmpq(reference, Immediate(Heap::kTagNil));
  if (is_nil != NULL) jmp(kEq, is_nil);
  if (not_nil != NULL) jmp(kNe, not_nil);
}


void Masm::IsUnboxed(Register reference, Label* not_unboxed, Label* boxed) {
  testb(reference, Immediate(0x01));
  if (not_unboxed != NULL) jmp(kEq, not_unboxed);
  if (boxed != NULL) jmp(kNe, boxed);
}


void Masm::IsHeapObject(Heap::HeapTag tag,
                        Register reference,
                        Label* mismatch,
                        Label* match) {
  Operand qtag(reference, 0);
  cmpb(qtag, Immediate(tag));
  if (mismatch != NULL) jmp(kNe, mismatch);
  if (match != NULL) jmp(kEq, match);
}


void Masm::IsTrue(Register reference, Label* is_false, Label* is_true) {
  // reference is definitely a boolean value
  // so no need to check it's type here
  Operand bvalue(reference, 8);
  cmpb(bvalue, Immediate(0));
  if (is_false != NULL) jmp(kEq, is_false);
  if (is_true != NULL) jmp(kNe, is_true);
}


void Masm::UnboxNumber(Register number) {
  assert(0 && "Not implemented");
}


void Masm::StoreRootStack() {
  Immediate root_stack(reinterpret_cast<uint64_t>(heap()->root_stack()));
  Operand scratch_op(scratch, 0);
  movq(scratch, root_stack);
  movq(scratch_op, rbp);
}


void Masm::Throw(Heap::Error error) {
  movq(rax, Immediate(error));
  Call(stubs()->GetThrowStub());
}


void Masm::Call(Register addr) {
  while ((offset() & 0xf) != 0xd) {
    nop();
  }
  callq(addr);
  nop();
}


void Masm::Call(Operand& addr) {
  while ((offset() & 0xf) != 0x9) {
    nop();
  }
  callq(addr);
  nop();
}


void Masm::Call(Register fn, uint32_t args) {
  Operand context_slot(fn, 8);
  Operand code_slot(fn, 16);
  movq(rdi, context_slot);
  movq(rsi, Immediate(args));

  Call(code_slot);
}


void Masm::Call(BaseStub* stub) {
  movq(scratch, Immediate(0));
  stub->Use(offset());

  Call(scratch);
}

} // namespace dotlang
