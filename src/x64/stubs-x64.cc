#include "stubs.h"
#include "macroassembler-x64.h"
#include "runtime.h"

namespace dotlang {

#define __ masm()->

BaseStub::BaseStub(Masm* masm, StubType type) : FFunction(masm),
                                                type_(type) {
}


void BaseStub::GeneratePrologue() {
  __ push(rbp);
  __ movq(rbp, rsp);
}


void BaseStub::GenerateEpilogue(int args) {
  __ movq(rsp, rbp);
  __ pop(rbp);

  // tag + size
  __ ret(args * 8);
}


void AllocateStub::Generate() {
  GeneratePrologue();
  // Align stack
  __ subq(rsp, Immediate(8));
  __ push(rbx);

  // Arguments
  Operand size(rbp, 24);
  Operand tag(rbp, 16);

  Label runtime_allocate(masm()), done(masm());

  Heap* heap = masm()->heap();
  Immediate heapref(reinterpret_cast<uint64_t>(heap));
  Immediate top(reinterpret_cast<uint64_t>(heap->new_space()->top()));
  Immediate limit(reinterpret_cast<uint64_t>(heap->new_space()->limit()));

  Operand scratch_op(scratch, 0);

  // Get pointer to current page's top
  // (new_space()->top() is a pointer to space's property
  // which is a pointer to page's top pointer
  // that's why we are dereferencing it here twice
  __ movq(scratch, top);
  __ movq(scratch, scratch_op);
  __ movq(rax, scratch_op);
  __ movq(rbx, size);
  __ Untag(rbx);

  // Add object size to the top
  __ addq(rbx, rax);
  __ jmp(kCarry, &runtime_allocate);

  // Check if we exhausted buffer
  __ movq(scratch, limit);
  __ movq(scratch, scratch_op);
  __ cmpq(rbx, scratch_op);
  __ jmp(kGt, &runtime_allocate);

  // We should allocate only even amount of bytes
  Label already_even(masm());

  __ testb(rbx, Immediate(0x01));
  __ jmp(kEq, &already_even);

  // Add one byte
  __ inc(rbx);

  __ bind(&already_even);

  // Update top
  __ movq(scratch, top);
  __ movq(scratch, scratch_op);
  __ movq(scratch_op, rbx);

  __ jmp(&done);

  // Invoke runtime allocation stub (and probably GC)
  __ bind(&runtime_allocate);

  // Remove junk from registers
  __ xorq(rax, rax);
  __ xorq(rbx, rbx);

  RuntimeAllocateCallback allocate = &RuntimeAllocate;

  {
    Label call(masm());

    Masm::Align a(masm_);
    __ Pushad();

    // Three arguments: heap, size, top_stack
    __ movq(rdi, heapref);
    __ movq(rsi, size);
    __ movq(rdx, rsp);

    // Objects are allocated in pairs with maps
    // do not let GC run in the middle of the process
    __ cmpb(tag, Immediate(masm()->TagNumber(Heap::kTagMap)));
    __ jmp(kNe, &call);
    __ xorq(rdx, rdx);

    __ bind(&call);
    __ movq(scratch, Immediate(*reinterpret_cast<uint64_t*>(&allocate)));

    __ callq(scratch);
    __ Popad(rax);
  }

  // Voila result and result_end are pointers
  __ bind(&done);

  // Set tag
  Operand qtag(rax, 0);
  __ movq(scratch, tag);
  __ Untag(scratch);
  __ movq(qtag, scratch);

  // Rax will hold resulting pointer
  __ pop(rbx);
  GenerateEpilogue(2);
}


void CoerceTypeStub::Generate() {
  GeneratePrologue();
  __ subq(rsp, Immediate(8));
  __ push(rbx);

  // Arguments
  Operand lhs(rbp, 16);
  Operand rhs(rbp, 24);

  Label done(masm()), not_number(masm());
  __ movq(rax, Immediate(0));

  // Get both values
  __ movq(rbx, lhs);
  __ movq(rax, rhs);

  // Check if their tags are equal (just return second in that case)
  Operand qtag_lhs(rbx, 0), qtag_rhs(rax, 0);
  __ movq(scratch, qtag_lhs);
  __ cmpb(scratch, qtag_rhs);
  __ jmp(kEq, &done);

  // If left is number
  __ cmpb(qtag_lhs, Immediate(Heap::kTagNumber));
  __ jmp(kNe, &not_number);

  {
    // TODO: Coerce right to number
    __ emitb(0xcc);
  }

  __ jmp(&done);

  __ bind(&not_number);

  {
    // TODO: Coerce right to string
    __ emitb(0xcc);
  }

  __ bind(&done);

  // Rax will hold resulting pointer

  __ pop(rbx);
  GenerateEpilogue(2);
}


void ThrowStub::Generate() {
  Immediate pending_exception(
      reinterpret_cast<uint64_t>(masm()->heap()->pending_exception()));
  Immediate root_stack(
      reinterpret_cast<uint64_t>(masm()->heap()->root_stack()));

  // Arguments: rax - exception num

  // Set pending exception
  Operand scratch_op(scratch, 0);
  __ movq(scratch, pending_exception);
  __ movq(scratch_op, rax);

  // Unwind stack to the top handler
  __ movq(scratch, root_stack);
  __ movq(rsp, scratch_op);

  // Return NULL
  __ movq(rax, 0);

  // Leave to C++ land
  __ pop(r15);
  __ pop(r14);
  __ pop(r13);
  __ pop(r12);
  __ pop(rbx);
  __ pop(rbp);
  __ ret(0);
}


void LookupPropertyStub::Generate() {
  GeneratePrologue();
  RuntimeLookupPropertyCallback lookup = &RuntimeLookupProperty;

  // Arguments
  Operand object(rbp, 32);
  Operand property(rbp, 24);
  Operand change(rbp, 16);

  __ Pushad();

  // RuntimeLookupProperty(heap, stack_top, obj, key, change)
  // (returns addr of slot)
  __ movq(rdi, Immediate(reinterpret_cast<uint64_t>(masm()->heap())));
  __ movq(rsi, rsp);
  __ movq(rdx, object);
  __ movq(rcx, property);
  __ movq(r8, change);
  __ movq(rax, Immediate(*reinterpret_cast<uint64_t*>(&lookup)));
  __ callq(rax);

  __ Popad(rax);
  GenerateEpilogue(3);
}


void CoerceToBooleanStub::Generate() {
  GeneratePrologue();
  RuntimeToBooleanCallback to_boolean = &RuntimeToBoolean;

  // Arguments
  Operand object(rbp, 16);

  __ Pushad();

  __ movq(rdi, Immediate(reinterpret_cast<uint64_t>(masm()->heap())));
  __ movq(rsi, rsp);
  __ movq(rdx, object);
  __ movq(rax, Immediate(*reinterpret_cast<uint64_t*>(&to_boolean)));
  __ callq(rax);

  __ Popad(rax);
  GenerateEpilogue(1);
}


void BinaryAddStub::Generate() {
  GeneratePrologue();
  __ subq(rsp, Immediate(8));
  __ push(rbx);

  // Arguments
  Operand lhs(rbp, 24);
  Operand rhs(rbp, 16);

  Label call_runtime(masm()), done(masm());

  __ movq(rax, lhs);
  __ movq(rbx, rhs);
  __ IsHeapObject(Heap::kTagNumber, rax, &call_runtime, NULL);
  __ IsHeapObject(Heap::kTagNumber, rbx, &call_runtime, NULL);

  // We're adding two heap numbers
  Operand lvalue(rax, 8);
  Operand rvalue(rbx, 8);
  __ movq(rax, lvalue);
  __ movq(rbx, rvalue);
  __ movqd(xmm1, rax);
  __ movqd(xmm2, rbx);
  __ addqd(xmm1, xmm2);

  __ movqd(rbx, xmm1);
  __ AllocateNumber(rbx, rax);

  __ jmp(&done);
  __ bind(&call_runtime);

  // TODO: Implement me
  __ emitb(0xcc);

  __ bind(&done);

  __ pop(rbx);

  // Caller should unwind stack
  GenerateEpilogue(0);
}

} // namespace dotlang
