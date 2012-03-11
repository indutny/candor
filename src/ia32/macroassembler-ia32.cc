#include "macroassembler.h"

#include "code-space.h" // CodeSpace
#include "heap.h" // HeapValue
#include "heap-inl.h"

#include "stubs.h"
#include "utils.h" // ComputeHash

#include <stdlib.h> // NULL

namespace candor {
namespace internal {

Masm::Masm(CodeSpace* space) : result_(eax),
                               slot_(new Operand(eax, 0)),
                               space_(space),
                               align_(0) {
}


void Masm::Pushad() {
  // 6 registers to save
  push(eax);
  push(ebx);
  push(ecx);
  push(edx);
  push(rsi);
  push(rdi);
}


void Masm::Popad(Register preserve) {
  PreservePop(rdi, preserve);
  PreservePop(rsi, preserve);
  PreservePop(edx, preserve);
  PreservePop(ecx, preserve);
  PreservePop(ebx, preserve);
  PreservePop(eax, preserve);
}


void Masm::AlignCode() {
  offset_ = RoundUp(offset_, 16);
  Grow();
}


Masm::Align::Align(Masm* masm) : masm_(masm), align_(masm->align_) {
  if (align_ % 2 == 0) return;

  masm->push(Immediate(0));
  masm->align_ += 1;
}


Masm::Align::~Align() {
  if (align_ % 2 == 0) return;
  masm_->addq(esp, 8);
  masm_->align_ -= 1;
}


Masm::Spill::Spill(Masm* masm, Register src, Register preserve) :
    masm_(masm), src_(src), index_(0), empty_(preserve.is(src)) {
  if (empty_) return;

  index_ = masm->spill_index_++;
  Operand slot(eax, 0);
  masm->SpillSlot(index(), slot);
  masm->movl(slot, src);

  if (masm->spill_index_ > masm->spills_) masm->spills_ = masm->spill_index_;
}


Masm::Spill::~Spill() {
  if (empty_) return;
  masm()->spill_index_--;
}


void Masm::Spill::Unspill(Register dst) {
  if (empty_) return;

  Operand slot(eax, 0);
  masm()->SpillSlot(index(), slot);
  masm()->movl(dst, slot);
}


void Masm::Spill::Unspill() {
  return Unspill(src_);
}


void Masm::AllocateSpills(uint32_t spill_offset) {
  spill_offset_ = spill_offset;
  spills_ = 0;
  spill_index_ = 0;
  subl(esp, Immediate(0));
  spill_reloc_ = new RelocationInfo(RelocationInfo::kValue,
                                    RelocationInfo::kLong,
                                    offset() - 4);
  relocation_info_.Push(spill_reloc_);
}


void Masm::FinalizeSpills() {
  spill_reloc_->target(RoundUp((spills_ + 1) << 3, 16));
}


void Masm::Allocate(Heap::HeapTag tag,
                    Register size_reg,
                    uint32_t size,
                    Register result) {
  Spill eax_s(this, eax, result);

  // Two arguments
  ChangeAlign(2);
  {
    Align a(this);

    // Add tag size
    if (size_reg.is(reg_nil)) {
      movl(eax, Immediate(TagNumber(size + 8)));
    } else {
      movl(eax, size_reg);
      Untag(eax);
      addl(eax, Immediate(8));
      TagNumber(eax);
    }
    push(eax);
    movl(eax, Immediate(TagNumber(tag)));
    push(eax);

    Call(stubs()->GetAllocateStub());
    // Stub will unwind stack
  }
  ChangeAlign(-2);

  if (!result.is(eax)) {
    movl(result, eax);
    eax_s.Unspill();
  }
}


void Masm::AllocateContext(uint32_t slots) {
  Spill eax_s(this, eax);

  // parent + number of slots + slots
  Allocate(Heap::kTagContext, reg_nil, 8 * (slots + 2), eax);

  // Move address of current context to first slot
  Operand lparent(eax, 8);
  movl(qparent, rdi);

  // Save number of slots
  Operand lslots(eax, 16);
  movl(lslots, Immediate(slots));

  // Clear context
  for (uint32_t i = 0; i < slots; i++) {
    Operand lslot(eax, 24 + i * 8);
    movl(lslot, Immediate(0));
  }

  // Replace current context
  // (It'll be restored by caller)
  movl(rdi, eax);
  eax_s.Unspill();

  CheckGC();
}


void Masm::AllocateFunction(Register addr, Register result) {
  // context + code
  Allocate(Heap::kTagFunction, reg_nil, 8 * 3, result);

  // Move address of current context to first slot
  Operand lparent(result, 8);
  Operand laddr(result, 16);
  Operand lroot(result, 24);
  movl(lparent, rdi);
  movl(laddr, addr);
  movl(lroot, root_reg);

  CheckGC();
}


void Masm::AllocateNumber(DoubleRegister value, Register result) {
  Allocate(Heap::kTagNumber, reg_nil, 8, result);

  Operand lvalue(result, 8);
  movld(lvalue, value);

  CheckGC();
}


void Masm::AllocateString(const char* value,
                          uint32_t length,
                          Register result) {
  // hash(8) + length(8)
  Allocate(Heap::kTagString, reg_nil, 16 + length, result);

  Operand lhash(result, 8);
  Operand llength(result, 16);

  movl(lhash, Immediate(ComputeHash(value, length)));
  movl(llength, Immediate(length));

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

  CheckGC();
}


void Masm::AllocateObjectLiteral(Heap::HeapTag tag,
                                 Register size,
                                 Register result) {
  // mask + map
  Allocate(tag,
           reg_nil,
           tag == Heap::kTagArray ? 24 : 16,
           result);

  Operand lmask(result, 8);
  Operand lmap(result, 16);

  // Array only field
  Operand llength(result, 24);

  // Set mask
  movq(scratch, size);

  // mask (= (size - 1) << 3)
  Untag(scratch);
  dec(scratch);
  shl(scratch, Immediate(3));
  movl(lmask, scratch);
  xorl(scratch, scratch);

  // Create map
  Spill size_s(this, size);

  Untag(size);
  // keys + values
  shl(size, Immediate(4));
  // + size
  addl(size, Immediate(8));
  TagNumber(size);

  Allocate(Heap::kTagMap, size, 0, scratch);
  movl(lmap, scratch);

  size_s.Unspill();
  Spill result_s(this, result);
  movl(result, scratch);

  // Save map size for GC
  Operand lmapsize(result, 8);
  Untag(size);
  movl(lmapsize, size);

  // Fill map with nil
  shl(size, Immediate(4));
  addl(result, Immediate(16));
  addl(size, result);
  Fill(result, size, Immediate(Heap::kTagNil));

  result_s.Unspill();
  size_s.Unspill();

  // Set length
  if (tag == Heap::kTagArray) {
    movl(llength, Immediate(0));
  }

  CheckGC();
}


void Masm::Fill(Register start, Register end, Immediate value) {
  Push(start);
  movl(scratch, value);

  Label entry(this), loop(this);
  jmp(&entry);
  bind(&loop);

  // Fill
  Operand op(start, 0);
  movl(op, scratch);

  // Move
  addl(start, Immediate(8));

  bind(&entry);

  // And loop
  cmpl(start, end);
  jmp(kLt, &loop);

  Pop(start);
  xorl(scratch, scratch);
}


void Masm::FillStackSlots() {
  movl(eax, esp);
  Fill(eax, ebp, Immediate(Heap::kTagNil));
  xorl(eax, eax);
}


void Masm::EnterFramePrologue() {
  Immediate last_stack(reinterpret_cast<uint64_t>(heap()->last_stack()));
  Operand scratch_op(scratch, 0);

  movl(scratch, last_stack);
  push(scratch_op);
  push(Immediate(0xFEEDBEEF));
}


void Masm::EnterFrameEpilogue() {
  addl(esp, Immediate(16));
}


void Masm::ExitFramePrologue() {
  Immediate last_stack(reinterpret_cast<uint64_t>(heap()->last_stack()));
  Operand scratch_op(scratch, 0);

  movl(scratch, last_stack);
  push(scratch_op);
  push(Immediate(0));
  movl(scratch_op, esp);
  xorl(scratch, scratch);
}


void Masm::ExitFrameEpilogue() {
  pop(scratch);
  pop(scratch);

  Immediate last_stack(reinterpret_cast<uint64_t>(heap()->last_stack()));
  Operand scratch_op(scratch, 0);

  // Restore previous last_stack
  push(eax);

  movl(eax, scratch);
  movl(scratch, last_stack);
  movl(scratch_op, eax);

  pop(eax);
}


void Masm::CheckGC() {
  Immediate gc_flag(reinterpret_cast<uint64_t>(heap()->needs_gc_addr()));
  Operand scratch_op(scratch, 0);

  Label done(this);

  // Check needs_gc flag
  movl(scratch, gc_flag);
  cmpb(scratch_op, Immediate(0));
  jmp(kEq, &done);

  Call(stubs()->GetCollectGarbageStub());

  bind(&done);
}


void Masm::IsNil(Register reference, Label* not_nil, Label* is_nil) {
  cmpl(reference, Immediate(Heap::kTagNil));
  if (is_nil != NULL) jmp(kEq, is_nil);
  if (not_nil != NULL) jmp(kNe, not_nil);
}


void Masm::IsUnboxed(Register reference, Label* not_unboxed, Label* unboxed) {
  testb(reference, Immediate(0x01));
  if (not_unboxed != NULL) jmp(kEq, not_unboxed);
  if (unboxed != NULL) jmp(kNe, unboxed);
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


void Masm::Call(Register addr) {
  while ((offset() & 0x1) != 0x1) {
    nop();
  }
  calll(addr);
  nop();
}


void Masm::Call(Operand& addr) {
  while ((offset() & 0x1) != 0x1) {
    nop();
  }
  calll(addr);
  nop();
}


void Masm::Call(Register fn, uint32_t args) {
  Operand context_slot(fn, 8);
  Operand code_slot(fn, 16);
  Operand root_slot(fn, 24);

  Label binding(this), done(this);

  movl(rdi, context_slot);
  movl(rsi, Immediate(TagNumber(args)));
  movl(root_reg, root_slot);

  // TODO: Use const here
  cmpl(rdi, Immediate(0x0DEF0DEF));
  jmp(kEq, &binding);

  Call(code_slot);

  jmp(&done);
  bind(&binding);

  push(rsi);
  push(fn);
  Call(stubs()->GetCallBindingStub());

  bind(&done);
}


void Masm::Call(char* stub) {
  movq(scratch, reinterpret_cast<uint64_t>(stub));

  Call(scratch);
}

} // namespace internal
} // namespace candor
