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
                               spill_offset_(8),
                               spill_index_(0),
                               spills_(0),
                               spill_operand_(rbp, 0) {
}


void Masm::Pushad() {
  // 12 registers to save
  push(rax);
  push(rbx);
  push(rcx);
  push(rdx);

  push(r8);
  push(r9);
  push(r10);
  push(r11);

  push(r12);
  push(r13);
  push(root_reg);
  push(context_reg);
}


void Masm::Popad(Register preserve) {
  PreservePop(context_reg, preserve);
  PreservePop(root_reg, preserve);
  PreservePop(r13, preserve);
  PreservePop(r12, preserve);

  PreservePop(r11, preserve);
  PreservePop(r10, preserve);
  PreservePop(r9, preserve);
  PreservePop(r8, preserve);

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

  FillStackSlots();
}


void Masm::FinalizeSpills() {
  if (spill_reloc_ == NULL) return;

  spill_reloc_->target(RoundUp((spill_offset_ + spills_ + 1) << 3, 16));
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
  mov(qparent, context_reg);

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
  mov(context_reg, rax);
  rax_s.Unspill();

  CheckGC();
}


void Masm::AllocateNumber(DoubleRegister value, Register result) {
  Allocate(Heap::kTagNumber, reg_nil, sizeof(double), result);

  Operand qvalue(result, HNumber::kValueOffset);
  movd(qvalue, value);
}


void Masm::AllocateObjectLiteral(Heap::HeapTag tag,
                                 Register tag_reg,
                                 Register size,
                                 Register result) {
  Operand qmask(result, HObject::kMaskOffset);
  Operand qmap(result, HObject::kMapOffset);

  // Array only field
  Operand qlength(result, HArray::kLengthOffset);

  if (tag_reg.is(reg_nil)) {
    // mask + map
    Allocate(tag,
             reg_nil,
             (tag == Heap::kTagArray ? 3 : 2) * HValue::kPointerSize,
             result);

    // Set length
    if (tag == Heap::kTagArray) {
      mov(qlength, Immediate(0));
    }
  } else {
    Label array, allocate_map;

    cmpq(tag_reg, Immediate(HNumber::Tag(Heap::kTagArray)));
    jmp(kEq, &array);

    Allocate(Heap::kTagObject, reg_nil, 2 * HValue::kPointerSize, result);

    jmp(&allocate_map);
    bind(&array);

    Allocate(Heap::kTagArray, reg_nil, 3 * HValue::kPointerSize, result);
    mov(qlength, Immediate(0));

    bind(&allocate_map);
  }

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
  // + proto + size
  addq(size, Immediate(2 * HValue::kPointerSize));
  TagNumber(size);

  Allocate(Heap::kTagMap, size, 0, scratch);
  mov(qmap, scratch);

  size_s.Unspill();
  Spill result_s(this, result);
  mov(result, scratch);

  // Save map size for GC
  Operand qmapsize(result, HMap::kSizeOffset);
  Operand qmapproto(result, HMap::kProtoOffset);
  Untag(size);
  mov(qmapsize, size);
  mov(qmapproto, Immediate(0));

  // Fill map with nil
  shl(size, Immediate(4));
  addq(result, Immediate(HMap::kSpaceOffset));
  addq(size, result);
  subq(size, Immediate(HValue::kPointerSize));
  Fill(result, size, Immediate(Heap::kTagNil));

  result_s.Unspill();
  size_s.Unspill();

  CheckGC();
}


void Masm::Fill(Register start, Register end, Immediate value) {
  Push(start);
  mov(scratch, value);

  Label entry, loop;
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
  push(scratch);
  push(rsi);
  push(rdi);
  mov(rsi, rsp);
  mov(rdi, rbp);
  // Skip rsi/rdi/scratch
  addq(rsi, Immediate(8 * 3));
  // Skip frame info
  subq(rdi, Immediate(8 * 1));
  Fill(rsi, rdi, Immediate(Heap::kTagNil));
  pop(rdi);
  pop(rsi);
  pop(scratch);
}


void Masm::EnterFramePrologue() {
  Immediate last_stack(reinterpret_cast<intptr_t>(heap()->last_stack()));
  Immediate last_frame(reinterpret_cast<intptr_t>(heap()->last_frame()));
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
  Immediate last_stack(reinterpret_cast<intptr_t>(heap()->last_stack()));
  Immediate last_frame(reinterpret_cast<intptr_t>(heap()->last_frame()));
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
  Immediate last_stack(reinterpret_cast<intptr_t>(heap()->last_stack()));
  Immediate last_frame(reinterpret_cast<intptr_t>(heap()->last_frame()));
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

  Label call_runtime, done;

  // Check if hash was already calculated
  mov(result, hash_field);
  cmpq(result, Immediate(0));
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
  Label loop_start, loop_cond, loop_end;

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
  Immediate gc_flag(reinterpret_cast<intptr_t>(heap()->needs_gc_addr()));
  Operand scratch_op(scratch, 0);

  Label done;

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
  Operand qmap(reference, HObject::kMapOffset);
  Operand qmapsize(scratch, HMap::kSizeOffset);
  mov(scratch, qmap);
  cmpq(qmapsize, Immediate(HArray::kDenseLengthMax));
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
  mov(scratch, reinterpret_cast<intptr_t>(stub));

  Call(scratch);
}


void Masm::CallFunction(Register fn) {
  Operand context_slot(fn, HFunction::kParentOffset);
  Operand code_slot(fn, HFunction::kCodeOffset);
  Operand root_slot(fn, HFunction::kRootOffset);

  Label binding, done;
  mov(context_reg, context_slot);
  mov(root_reg, root_slot);

  cmpq(context_reg, Immediate(Heap::kBindingContextTag));
  jmp(kEq, &binding);

  Call(code_slot);

  jmp(&done);
  bind(&binding);

  // Push argc and function
  push(rax);
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
