#include "macroassembler.h"

#include "code-space.h" // CodeSpace
#include "heap.h" // HeapValue
#include "heap-inl.h"

#include "stubs.h"
#include "utils.h" // ComputeHash

#include <stdlib.h> // NULL

namespace candor {
namespace internal {

Masm::Masm(CodeSpace* space) : space_(space),
                               align_(0),
                               spill_reloc_(NULL),
                               spill_operand_(rbp, 0) {
}


void Masm::Pushad() {
  // 10 registers to save (10 * 8 = 16 * 5, so stack should be aligned)
  push(rax);
  push(rbx);
  push(rcx);
  push(rdx);
  push(rsi);
  push(rdi);
  push(r8);
  push(r9);
  // Root register
  push(root_reg);
  push(r12);
}


void Masm::Popad(Register preserve) {
  PreservePop(r12, preserve);
  PreservePop(root_reg, preserve);
  PreservePop(r9, preserve);
  PreservePop(r8, preserve);
  PreservePop(rdi, preserve);
  PreservePop(rsi, preserve);
  PreservePop(rdx, preserve);
  PreservePop(rcx, preserve);
  PreservePop(rbx, preserve);
  PreservePop(rax, preserve);
}


void Masm::AlignCode() {
  offset_ = RoundUp(offset_, 16);
  Grow();
}


Masm::Align::Align(Masm* masm) : masm_(masm), align_(masm->align_) {
  if (align_ % 2 == 0) return;

  masm->push(Immediate(Heap::kTagNil));
  masm->align_ += 1;
}


Masm::Align::~Align() {
  if (align_ % 2 == 0) return;
  masm_->addq(rsp, 8);
  masm_->align_ -= 1;
}


Masm::Spill::Spill(Masm* masm) : masm_(masm),
                                 src_(reg_nil),
                                 index_(0) {
}


Masm::Spill::Spill(Masm* masm, Register src) : masm_(masm),
                                               src_(reg_nil),
                                               index_(0) {
  SpillReg(src);
}


void Masm::Spill::SpillReg(Register src) {
  if (is_empty()) {
    index_ = masm()->spill_index_++;
  }

  src_ = src;
  Operand slot(rax, 0);
  masm()->SpillSlot(index(), slot);
  masm()->mov(slot, src);

  if (masm()->spill_index_ > masm()->spills_) {
    masm()->spills_ = masm()->spill_index_;
  }
}


Masm::Spill::~Spill() {
  if (!is_empty()) {
    masm()->spill_index_--;
  }
}


void Masm::Spill::Unspill(Register dst) {
  assert(!is_empty());

  Operand slot(rax, 0);
  masm()->SpillSlot(index(), slot);
  masm()->mov(dst, slot);
}


void Masm::Spill::Unspill() {
  return Unspill(src_);
}


void Masm::AllocateSpills() {
  subq(rsp, Immediate(0));
  spill_reloc_ = new RelocationInfo(RelocationInfo::kValue,
                                    RelocationInfo::kLong,
                                    offset() - 4);

  relocation_info_.Push(spill_reloc_);
}


void Masm::FinalizeSpills(int spills_count) {
  if (spill_reloc_ == NULL) return;

  spill_reloc_->target(8 + RoundUp((spills_count + spills_ + 1) << 3, 16));
}


void Masm::Allocate(Heap::HeapTag tag,
                    Register size_reg,
                    uint32_t size,
                    Register result) {
  if (!result.is(rax)) {
    push(rax);
    push(rax);
  }

  // Add tag size
  if (size_reg.is(reg_nil)) {
    mov(rax, Immediate(HNumber::Tag(size + HValue::kPointerSize)));
  } else {
    mov(rax, size_reg);
    Untag(rax);
    addq(rax, Immediate(HValue::kPointerSize));
    TagNumber(rax);
  }
  push(rax);
  mov(rax, Immediate(HNumber::Tag(tag)));
  push(rax);

  Call(stubs()->GetAllocateStub());
  // Stub will unwind stack
  //
  if (!result.is(rax)) {
    mov(result, rax);
    pop(rax);
    pop(rax);
  }
}


void Masm::AllocateContext(uint32_t slots) {
  Spill rax_s(this, rax);

  // parent + number of slots + slots
  Allocate(Heap::kTagContext, reg_nil, HValue::kPointerSize * (slots + 2), rax);

  // Move address of current context to first slot
  Operand qparent(rax, HContext::kParentOffset);
  mov(qparent, rdi);

  // Save number of slots
  Operand qslots(rax, HContext::kSlotsOffset);
  mov(qslots, Immediate(slots));

  // Clear context
  for (uint32_t i = 0; i < slots; i++) {
    Operand qslot(rax, HContext::GetIndexDisp(i));
    mov(qslot, Immediate(Heap::kTagNil));
  }

  // Replace current context
  // (It'll be restored by caller)
  mov(rdi, rax);
  rax_s.Unspill();

  CheckGC();
}


void Masm::AllocateNumber(DoubleRegister value, Register result) {
  Allocate(Heap::kTagNumber, reg_nil, HValue::kPointerSize, result);

  Operand qvalue(result, HNumber::kValueOffset);
  movd(qvalue, value);

  CheckGC();
}


void Masm::AllocateObjectLiteral(Heap::HeapTag tag,
                                 Register size,
                                 Register result) {
  // mask + map
  Allocate(tag,
           reg_nil,
           (tag == Heap::kTagArray ? 3 : 2) * HValue::kPointerSize,
           result);

  Operand qmask(result, HObject::kMaskOffset);
  Operand qmap(result, HObject::kMapOffset);

  // Array only field
  Operand qlength(result, HArray::kLengthOffset);

  // Set mask
  mov(scratch, size);

  // mask (= (size - 1) << 3)
  Untag(scratch);
  dec(scratch);
  shl(scratch, Immediate(3));
  mov(qmask, scratch);
  xorq(scratch, scratch);

  // Create map
  Spill size_s(this, size);

  Untag(size);
  // keys + values
  shl(size, Immediate(4));
  // + size
  addq(size, Immediate(HValue::kPointerSize));
  TagNumber(size);

  Allocate(Heap::kTagMap, size, 0, scratch);
  mov(qmap, scratch);

  size_s.Unspill();
  Spill result_s(this, result);
  mov(result, scratch);

  // Save map size for GC
  Operand qmapsize(result, HMap::kSizeOffset);
  Untag(size);
  mov(qmapsize, size);

  // Fill map with nil
  shl(size, Immediate(4));
  addq(result, Immediate(HMap::kSpaceOffset));
  addq(size, result);
  subq(size, Immediate(HValue::kPointerSize));
  Fill(result, size, Immediate(Heap::kTagNil));

  result_s.Unspill();
  size_s.Unspill();

  // Set length
  if (tag == Heap::kTagArray) {
    mov(qlength, Immediate(0));
  }

  CheckGC();
}


void Masm::AllocateVarArgSlots(Spill* vararg, Register argc) {
  Operand qlength(scratch, HArray::kLengthOffset);
  vararg->Unspill(scratch);
  mov(scratch, qlength);

  // Increase argc
  TagNumber(scratch);
  addq(argc, scratch);

  // Push RoundUp(scratch, 2) nils to stack
  Label loop_start(this), loop_cond(this);

  mov(rax, Immediate(Heap::kTagNil));

  testb(scratch, Immediate(HNumber::Tag(1)));
  jmp(kEq, &loop_cond);

  // Additional push for alignment
  push(rax);

  jmp(&loop_cond);
  bind(&loop_start);

  push(rax);
  subq(scratch, Immediate(HNumber::Tag(1)));

  bind(&loop_cond);
  cmpq(scratch, Immediate(HNumber::Tag(0)));
  jmp(kNe, &loop_start);
}


void Masm::Fill(Register start, Register end, Immediate value) {
  Push(start);
  mov(scratch, value);

  Label entry(this), loop(this);
  jmp(&entry);
  bind(&loop);

  // Fill
  Operand op(start, 0);
  mov(op, scratch);

  // Move
  addq(start, Immediate(8));

  bind(&entry);

  // And loop
  cmpq(start, end);
  jmp(kLe, &loop);

  Pop(start);
  xorq(scratch, scratch);
}


void Masm::FillStackSlots() {
  mov(rax, rsp);
  mov(rbx, rbp);
  // Skip frame info
  subq(rbx, Immediate(8));
  Fill(rax, rbx, Immediate(Heap::kTagNil));
  xorq(rax, rax);
  xorq(rbx, rbx);
}


void Masm::EnterFramePrologue() {
  Immediate last_stack(reinterpret_cast<uint64_t>(heap()->last_stack()));
  Immediate last_frame(reinterpret_cast<uint64_t>(heap()->last_frame()));
  Operand scratch_op(scratch, 0);

  push(Immediate(Heap::kTagNil));
  mov(scratch, last_frame);
  push(scratch_op);
  mov(scratch, last_stack);
  push(scratch_op);
  push(Immediate(Heap::kEnterFrameTag));
}


void Masm::EnterFrameEpilogue() {
  addq(rsp, Immediate(4 << 3));
}


void Masm::ExitFramePrologue() {
  Immediate last_stack(reinterpret_cast<uint64_t>(heap()->last_stack()));
  Immediate last_frame(reinterpret_cast<uint64_t>(heap()->last_frame()));
  Operand scratch_op(scratch, 0);

  mov(scratch, last_frame);
  push(scratch_op);
  mov(scratch_op, rbp);

  mov(scratch, last_stack);
  push(scratch_op);
  mov(scratch_op, rsp);
  xorq(scratch, scratch);
}


void Masm::ExitFrameEpilogue() {
  Immediate last_stack(reinterpret_cast<uint64_t>(heap()->last_stack()));
  Immediate last_frame(reinterpret_cast<uint64_t>(heap()->last_frame()));
  Operand scratch_op(scratch, 0);

  pop(scratch);

  // Restore previous last_stack
  // NOTE: we can safely use rbx here, look at stubs-x64.cc
  mov(rbx, scratch);
  mov(scratch, last_stack);
  mov(scratch_op, rbx);

  pop(scratch);

  // Restore previous last_frame
  mov(rbx, scratch);
  mov(scratch, last_frame);
  mov(scratch_op, rbx);
}


void Masm::StringHash(Register str, Register result) {
  Operand hash_field(str, HString::kHashOffset);
  Operand repr_field(str, HValue::kRepresentationOffset);

  Label call_runtime(this), done(this);

  // Check if hash was already calculated
  mov(scratch, hash_field);
  cmpq(scratch, Immediate(0));
  jmp(kNe, &done);

  // Check if string is a cons string
  movzxb(scratch, repr_field);
  cmpb(scratch, Immediate(HString::kNormal));
  jmp(kNe, &call_runtime);

  // Compute new hash
  assert(!str.is(rcx));
  if (!result.is(rcx)) push(rcx);
  push(str);
  push(rsi);

  Register scratch = rsi;

  // hash = 0
  xorq(result, result);

  // rcx = length
  Operand length_field(str, HString::kLengthOffset);
  mov(rcx, length_field);

  // str += kValueOffset
  addq(str, Immediate(HString::kValueOffset));

  // while (rcx != 0)
  Label loop_start(this), loop_cond(this), loop_end(this);

  jmp(&loop_cond);
  bind(&loop_start);

  Operand ch(str, 0);

  // result += str[0]
  movzxb(scratch, ch);
  addl(result, scratch);

  // result += result << 10
  mov(scratch, result);
  shll(result, Immediate(10));
  addl(result, scratch);

  // result ^= result >> 6
  mov(scratch, result);
  shrl(result, Immediate(6));
  xorl(result, scratch);

  // rcx--; str++
  dec(rcx);
  inc(str);

  bind(&loop_cond);

  // check condition (rcx != 0)
  cmpq(rcx, Immediate(0));
  jmp(kNe, &loop_start);

  bind(&loop_end);

  // Mixup
  // result += (result << 3);
  mov(scratch, result);
  shll(result, Immediate(3));
  addl(result, scratch);

  // result ^= (result >> 11);
  mov(scratch, result);
  shrl(result, Immediate(11));
  xorl(result, scratch);

  // result += (result << 15);
  mov(scratch, result);
  shll(result, Immediate(15));
  addl(result, scratch);

  pop(rsi);
  pop(str);
  if (!result.is(rcx)) pop(rcx);

  // Store hash into a string
  mov(hash_field, result);

  jmp(&done);
  bind(&call_runtime);

  push(rax);
  push(str);
  Call(stubs()->GetHashValueStub());
  pop(str);

  if (result.is(rax)) {
    pop(scratch);
  } else {
    mov(result, rax);
    pop(rax);
  }

  bind(&done);
}


void Masm::CheckGC() {
  Immediate gc_flag(reinterpret_cast<uint64_t>(heap()->needs_gc_addr()));
  Operand scratch_op(scratch, 0);

  Label done(this);

  // Check needs_gc flag
  mov(scratch, gc_flag);
  cmpb(scratch_op, Immediate(0));
  jmp(kEq, &done);

  Call(stubs()->GetCollectGarbageStub());

  bind(&done);
}


void Masm::IsNil(Register reference, Label* not_nil, Label* is_nil) {
  cmpq(reference, Immediate(Heap::kTagNil));
  if (is_nil != NULL) jmp(kEq, is_nil);
  if (not_nil != NULL) jmp(kNe, not_nil);
}


void Masm::IsUnboxed(Register reference, Label* not_unboxed, Label* unboxed) {
  testb(reference, Immediate(0x01));
  if (not_unboxed != NULL) jmp(kNe, not_unboxed);
  if (unboxed != NULL) jmp(kEq, unboxed);
}


void Masm::IsHeapObject(Heap::HeapTag tag,
                        Register reference,
                        Label* mismatch,
                        Label* match) {
  Operand qtag(reference, HValue::kTagOffset);
  cmpb(qtag, Immediate(tag));
  if (mismatch != NULL) jmp(kNe, mismatch);
  if (match != NULL) jmp(kEq, match);
}


void Masm::IsTrue(Register reference, Label* is_false, Label* is_true) {
  // reference is definitely a boolean value
  // so no need to check it's type here
  Operand bvalue(reference, HBoolean::kValueOffset);
  cmpb(bvalue, Immediate(0));
  if (is_false != NULL) jmp(kEq, is_false);
  if (is_true != NULL) jmp(kNe, is_true);
}


void Masm::IsDenseArray(Register reference, Label* non_dense, Label* dense) {
  Operand qlength(reference, HArray::kLengthOffset);
  cmpq(qlength, Immediate(HArray::kDenseLengthMax));
  if (non_dense != NULL) jmp(kGt, non_dense);
  if (dense != NULL) jmp(kLe, dense);
}


void Masm::Call(Register addr) {
  while ((offset() & 0x1) != 0x1) {
    nop();
  }
  callq(addr);
  nop();
}


void Masm::Call(Operand& addr) {
  while ((offset() & 0x1) != 0x1) {
    nop();
  }
  callq(addr);
  nop();
}


void Masm::Call(char* stub) {
  mov(scratch, reinterpret_cast<uint64_t>(stub));

  Call(scratch);
}


void Masm::CallFunction(Register fn) {
  Operand context_slot(fn, HFunction::kParentOffset);
  Operand code_slot(fn, HFunction::kCodeOffset);
  Operand root_slot(fn, HFunction::kRootOffset);

  Label binding(this), done(this);
  mov(rdi, context_slot);
  mov(root_reg, root_slot);

  cmpq(rdi, Immediate(Heap::kBindingContextTag));
  jmp(kEq, &binding);

  Call(code_slot);

  jmp(&done);
  bind(&binding);

  push(rsi);
  push(fn);
  Call(stubs()->GetCallBindingStub());

  bind(&done);
}


void Masm::ProbeCPU() {
  push(rbp);
  mov(rbp, rsp);

  push(rbx);
  push(rcx);
  push(rdx);

  mov(rax, Immediate(0x01));
  cpuid();
  mov(rax, rcx);

  pop(rdx);
  pop(rcx);
  pop(rbx);

  mov(rsp, rbp);
  pop(rbp);
  ret(0);
}

} // namespace internal
} // namespace candor
