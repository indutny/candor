#include "macroassembler.h"

#include "code-space.h" // CodeSpace
#include "heap.h" // HeapValue
#include "heap-inl.h"

#include "stubs.h"
#include "utils.h" // ComputeHash

#include <stdlib.h> // NULL

namespace candor {
namespace internal {

Masm::Masm(CodeSpace* space) : slot_(eax, 0),
                               space_(space),
                               align_(0) {
}


void Masm::Pushad() {
  // 8 registers to save (4 * 8 = 16 * 2, so stack should be aligned)
  push(eax);
  push(ebx);
  push(ecx);
  push(edx);

  push(esi);
  push(edi);
  push(edi);
  push(edi);
}


void Masm::Popad(Register preserve) {
  PreservePop(edi, preserve);
  PreservePop(edi, preserve);
  PreservePop(edi, preserve);
  PreservePop(esi, preserve);
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
  while (masm->align_ % 4 != 0) {
    masm->push(Immediate(Heap::kTagNil));
    masm->align_ += 1;
  }
}


Masm::Align::~Align() {
  masm_->addl(esp, Immediate((masm_->align_ - align_) * 4));
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
  Operand slot(eax, 0);
  masm()->SpillSlot(index(), slot);
  masm()->movl(slot, src);

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

  Operand slot(eax, 0);
  masm()->SpillSlot(index(), slot);
  masm()->movl(dst, slot);
}


void Masm::Spill::Unspill() {
  return Unspill(src_);
}


void Masm::AllocateSpills(uint32_t stack_slots) {
  // 2 slots or alignment
  spill_offset_ = RoundUp((stack_slots + 1) * 4, 16) + 2 * 4;
  spills_ = 0;
  spill_index_ = 0;
  subl(esp, Immediate(0));
  spill_reloc_ = new RelocationInfo(RelocationInfo::kValue,
                                    RelocationInfo::kLong,
                                    offset() - 4);

  relocation_info_.Push(spill_reloc_);
}


void Masm::FinalizeSpills() {
  spill_reloc_->target(spill_offset_ + RoundUp((spills_ + 1) << 2, 16));
}


void Masm::Allocate(Heap::HeapTag tag,
                    Register size_reg,
                    uint32_t size,
                    Register result) {
  Spill eax_s(this, eax);

  // Two arguments
  ChangeAlign(2);
  {
    Align a(this);

    // Add tag size
    if (size_reg.is(reg_nil)) {
      movl(eax, Immediate(HNumber::Tag(size + HValue::kPointerSize)));
    } else {
      movl(eax, size_reg);
      Untag(eax);
      addl(eax, Immediate(HValue::kPointerSize));
      TagNumber(eax);
    }
    push(eax);
    movl(eax, Immediate(HNumber::Tag(tag)));
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
  Allocate(Heap::kTagContext, reg_nil, HValue::kPointerSize * (slots + 2), eax);

  // Move address of current context to first slot
  Operand qparent(eax, HContext::kParentOffset);
  movl(qparent, edi);

  // Save number of slots
  Operand qslots(eax, HContext::kSlotsOffset);
  movl(qslots, Immediate(slots));

  // Clear context
  for (uint32_t i = 0; i < slots; i++) {
    Operand qslot(eax, HContext::GetIndexDisp(i));
    movl(qslot, Immediate(Heap::kTagNil));
  }

  // Replace current context
  // (It'll be restored by caller)
  movl(edi, eax);
  eax_s.Unspill();

  CheckGC();
}


void Masm::AllocateFunction(Register addr, Register result, uint32_t argc) {
  // context + code + root + argc
  Allocate(Heap::kTagFunction, reg_nil, HValue::kPointerSize * 4, result);

  // Move address of current context to first slot
  Operand qparent(result, HFunction::kParentOffset);
  Operand qaddr(result, HFunction::kCodeOffset);
  Operand qroot(result, HFunction::kRootOffset);
  Operand qargc(result, HFunction::kArgcOffset);
  movl(qparent, edi);
  movl(qaddr, addr);
  movl(addr, root_op);
  movl(qroot, addr);
  movl(qargc, Immediate(argc));

  xorl(addr, addr);

  CheckGC();
}


void Masm::AllocateNumber(DoubleRegister value, Register result) {
  Allocate(Heap::kTagNumber, reg_nil, HValue::kPointerSize, result);

  Operand qvalue(result, HNumber::kValueOffset);
  movld(qvalue, value);

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
  movl(scratch, size);

  // mask (= (size - 1) << 2)
  Untag(scratch);
  dec(scratch);
  shl(scratch, Immediate(3));
  movl(qmask, scratch);
  xorl(scratch, scratch);

  // Create map
  Spill size_s(this, size);

  Untag(size);
  // keys + values
  shl(size, Immediate(3));
  // + size
  addl(size, Immediate(HValue::kPointerSize));
  TagNumber(size);

  Allocate(Heap::kTagMap, size, 0, scratch);
  movl(qmap, scratch);

  size_s.Unspill();
  Spill result_s(this, result);
  movl(result, scratch);

  // Save map size for GC
  Operand qmapsize(result, HMap::kSizeOffset);
  Untag(size);
  movl(qmapsize, size);

  // Fill map with nil
  shl(size, Immediate(3));
  addl(result, Immediate(HMap::kSpaceOffset));
  addl(size, result);
  subl(size, Immediate(HValue::kPointerSize));
  Fill(result, size, Immediate(Heap::kTagNil));

  result_s.Unspill();
  size_s.Unspill();

  // Set length
  if (tag == Heap::kTagArray) {
    movl(qlength, Immediate(0));
  }

  CheckGC();
}


void Masm::AllocateVarArgSlots(Spill* vararg, Register argc) {
  Operand qlength(scratch, HArray::kLengthOffset);
  vararg->Unspill(scratch);
  movl(scratch, qlength);

  // Increase argc
  TagNumber(scratch);
  addl(argc, scratch);

  // Push RoundUp(scratch, 2) nils to stack
  Label loop_start(this), loop_cond(this);

  movl(eax, Immediate(Heap::kTagNil));

  testb(scratch, Immediate(HNumber::Tag(1)));
  jmp(kEq, &loop_cond);

  // Additional push for alignment
  push(eax);

  jmp(&loop_cond);
  bind(&loop_start);

  push(eax);
  subl(scratch, Immediate(HNumber::Tag(1)));

  bind(&loop_cond);
  cmpl(scratch, Immediate(HNumber::Tag(0)));
  jmp(kNe, &loop_start);
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
  addl(start, Immediate(4));

  bind(&entry);

  // And loop
  cmpl(start, end);
  jmp(kLe, &loop);

  Pop(start);
  xorl(scratch, scratch);
}


void Masm::FillStackSlots() {
  movl(eax, esp);
  movl(ecx, ebp);
  // Skip frame info
  subl(ecx, Immediate(4));
  Fill(eax, ecx, Immediate(Heap::kTagNil));
  xorl(eax, eax);
  xorl(ecx, ecx);
}


void Masm::EnterFramePrologue() {
  Immediate last_stack(reinterpret_cast<uint32_t>(heap()->last_stack()));
  Immediate last_frame(reinterpret_cast<uint32_t>(heap()->last_frame()));
  Operand scratch_op(scratch, 0);

  push(Immediate(Heap::kTagNil));
  movl(scratch, last_frame);
  push(scratch_op);
  movl(scratch, last_stack);
  push(scratch_op);
  push(Immediate(Heap::kEnterFrameTag));
}


void Masm::EnterFrameEpilogue() {
  addl(esp, Immediate(4 << 2));
}


void Masm::ExitFramePrologue() {
  Immediate last_stack(reinterpret_cast<uint32_t>(heap()->last_stack()));
  Immediate last_frame(reinterpret_cast<uint32_t>(heap()->last_frame()));
  Operand scratch_op(scratch, 0);

  // Just for alignment
  push(Immediate(Heap::kTagNil));
  push(Immediate(Heap::kTagNil));

  movl(scratch, last_frame);
  push(scratch_op);
  movl(scratch_op, ebp);

  movl(scratch, last_stack);
  push(scratch_op);
  movl(scratch_op, esp);
  xorl(scratch, scratch);
}


void Masm::ExitFrameEpilogue() {
  Immediate last_stack(reinterpret_cast<uint32_t>(heap()->last_stack()));
  Immediate last_frame(reinterpret_cast<uint32_t>(heap()->last_frame()));
  Operand scratch_op(scratch, 0);

  pop(scratch);

  // Restore previous last_stack
  // NOTE: we can safely use ebx here, look at stubs-ia32.cc
  movl(ebx, scratch);
  movl(scratch, last_stack);
  movl(scratch_op, ebx);

  pop(scratch);

  // Restore previous last_frame
  movl(ebx, scratch);
  movl(scratch, last_frame);
  movl(scratch_op, ebx);

  pop(scratch);
  pop(scratch);
}


void Masm::StringHash(Register str, Register result) {
  Operand hash_field(str, HString::kHashOffset);
  Operand repr_field(str, HValue::kRepresentationOffset);

  Label call_runtime(this), done(this);

  // Check if hash was already calculated
  movl(scratch, hash_field);
  cmpl(scratch, Immediate(0));
  jmp(kNe, &done);

  // Check if string is a cons string
  movzxb(scratch, repr_field);
  cmpb(scratch, Immediate(HString::kNormal));
  jmp(kNe, &call_runtime);

  // Compute new hash
  assert(!str.is(ecx));
  if (!result.is(ecx)) push(ecx);
  push(str);
  push(esi);

  Register scratch = esi;

  // hash = 0
  xorl(result, result);

  // ecx = length
  Operand length_field(str, HString::kLengthOffset);
  movl(ecx, length_field);

  // str += kValueOffset
  addl(str, Immediate(HString::kValueOffset));

  // while (ecx != 0)
  Label loop_start(this), loop_cond(this), loop_end(this);

  jmp(&loop_cond);
  bind(&loop_start);

  Operand ch(str, 0);

  // result += str[0]
  movzxb(scratch, ch);
  addl(result, scratch);

  // result += result << 10
  movl(scratch, result);
  shl(result, Immediate(10));
  addl(result, scratch);

  // result ^= result >> 6
  movl(scratch, result);
  shr(result, Immediate(6));
  xorl(result, scratch);

  // ecx--; str++
  dec(ecx);
  inc(str);

  bind(&loop_cond);

  // check condition (ecx != 0)
  cmpl(ecx, Immediate(0));
  jmp(kNe, &loop_start);

  bind(&loop_end);

  // Mixup
  // result += (result << 3);
  movl(scratch, result);
  shl(result, Immediate(3));
  addl(result, scratch);

  // result ^= (result >> 11);
  movl(scratch, result);
  shr(result, Immediate(11));
  xorl(result, scratch);

  // result += (result << 15);
  movl(scratch, result);
  shl(result, Immediate(15));
  addl(result, scratch);

  pop(esi);
  pop(str);
  if (!result.is(ecx)) pop(ecx);

  // Store hash into a string
  movl(hash_field, result);

  jmp(&done);
  bind(&call_runtime);

  push(eax);
  push(str);
  Call(stubs()->GetHashValueStub());
  pop(str);

  if (result.is(eax)) {
    pop(scratch);
  } else {
    movl(result, eax);
    pop(eax);
  }

  bind(&done);
}


void Masm::CheckGC() {
  Immediate gc_flag(reinterpret_cast<uint32_t>(heap()->needs_gc_addr()));
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
  cmpl(qlength, Immediate(HArray::kDenseLengthMax));
  if (non_dense != NULL) jmp(kGt, non_dense);
  if (dense != NULL) jmp(kLe, dense);
}


void Masm::Call(Register addr) {
  while ((offset() & 0x1) != 0x1) {
    nop();
  }
  call(addr);
  nop();
}


void Masm::Call(Operand& addr) {
  while ((offset() & 0x1) != 0x1) {
    nop();
  }
  call(addr);
  nop();
}


void Masm::Call(char* stub) {
  movl(scratch, reinterpret_cast<uint32_t>(stub));

  Call(scratch);
}


void Masm::CallFunction(Register fn) {
  Operand context_slot(fn, HFunction::kParentOffset);
  Operand code_slot(fn, HFunction::kCodeOffset);
  Operand root_slot(fn, HFunction::kRootOffset);

  Label binding(this), done(this);
  movl(edx, root_slot);
  movl(edi, context_slot);

  cmpl(edi, Immediate(Heap::kBindingContextTag));
  jmp(kEq, &binding);

  Call(code_slot);

  jmp(&done);
  bind(&binding);

  push(esi);
  push(fn);
  Call(stubs()->GetCallBindingStub());

  bind(&done);
}


void Masm::ProbeCPU() {
  push(ebp);
  movl(ebp, esp);

  push(ebx);
  push(ecx);
  push(edx);

  movl(eax, Immediate(0x01));
  cpuid();
  movl(eax, ecx);

  pop(edx);
  pop(ecx);
  pop(ebx);

  movl(esp, ebp);
  pop(ebp);
  ret(0);
}

} // namespace internal
} // namespace candor
