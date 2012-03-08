#include "stubs.h"
#include "cpu.h" // CPU
#include "ast.h" // BinOp
#include "macroassembler-x64.h" // Masm
#include "runtime.h"

namespace candor {
namespace internal {

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


void EntryStub::Generate() {
  // rdi <- root context
  // rsi <- unboxed arguments count (tagged)
  // rdx <- pointer to arguments array
  // rcx <- address of code chunk to call
  // r8 <- parent context

  // Store address of root context
  __ movq(root_reg, rdi);

  // Store registers
  __ push(rbp);
  __ push(rbx);
  __ push(r12);
  __ push(r13);
  __ push(r14);
  __ push(r15);

  __ EnterFramePrologue();

  __ StoreRootStack();

  // Push all arguments to stack
  Label even(masm()), args(masm()), args_loop(masm()), unwind_even(masm());
  __ movq(scratch, rsi);
  __ Untag(scratch);
  __ movq(rbx, rdx);

  // Odd arguments count check
  __ testb(scratch, Immediate(1));
  __ jmp(kEq, &even);
  __ push(Immediate(0));
  __ bind(&even);

  __ jmp(&args_loop);

  __ bind(&args);

  // Get argument from list
  Operand arg(rbx, 0);
  __ movq(rax, arg);
  __ push(rax);

  // Decrement count and move "finger"
  __ dec(scratch);
  __ addq(rbx, Immediate(8));

  // Loop if needed
  __ bind(&args_loop);
  __ cmpq(scratch, Immediate(0));
  __ jmp(kNe, &args);

  // Save code address
  __ movq(scratch, rcx);

  // Set context
  __ movq(rdi, r8);

  // Nullify all registers to help GC distinguish on-stack values
  __ xorq(rbp, rbp);
  __ xorq(rax, rax);
  __ xorq(rbx, rbx);
  __ xorq(rcx, rcx);
  __ xorq(rdx, rdx);
  __ xorq(r8, r8);
  __ xorq(r9, r9);
  // r10 is a root register
  __ xorq(r12, r12);
  // r11 is a scratch register
  __ xorq(r13, r13);
  __ xorq(r14, r14);
  __ xorq(r15, r15);

  // Call code
  __ Call(scratch);

  // Unwind arguments
  __ Untag(rsi);

  __ testb(rsi, Immediate(1));
  __ jmp(kEq, &unwind_even);
  __ inc(rsi);
  __ bind(&unwind_even);

  __ shl(rsi, Immediate(3));
  __ addq(rsp, rsi);
  __ xorq(rsi, rsi);

  __ RestoreRootStack();

  __ EnterFrameEpilogue();

  // Restore registers
  __ pop(r15);
  __ pop(r14);
  __ pop(r13);
  __ pop(r12);
  __ pop(rbx);
  __ pop(rbp);

  __ ret(0);
}


void AllocateStub::Generate() {
  GeneratePrologue();
  // Align stack
  __ push(Immediate(0));
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

  // Invoke runtime allocation stub
  __ bind(&runtime_allocate);

  // Remove junk from registers
  __ xorq(rax, rax);
  __ xorq(rbx, rbx);

  RuntimeAllocateCallback allocate = &RuntimeAllocate;

  {
    Masm::Align a(masm());
    __ Pushad();

    // Three arguments: heap, size
    __ movq(rdi, heapref);
    __ movq(rsi, size);

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


void CollectGarbageStub::Generate() {
  GeneratePrologue();

  RuntimeCollectGarbageCallback gc = &RuntimeCollectGarbage;
  __ Pushad();

  {
    Masm::Align a(masm());

    // RuntimeCollectGarbage(heap, stack_top)
    __ movq(rdi, Immediate(reinterpret_cast<uint64_t>(masm()->heap())));
    __ movq(rsi, rsp);
    __ movq(rax, Immediate(*reinterpret_cast<uint64_t*>(&gc)));
    __ Call(rax);
  }

  __ Popad(reg_nil);

  GenerateEpilogue(0);
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
  __ subq(rsp, Immediate(8));

  // Return NULL
  __ movq(rax, 0);

  // Leave to Entry Stub
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

  // RuntimeLookupProperty(heap, obj, key, change)
  // (returns addr of slot)
  __ movq(rdi, Immediate(reinterpret_cast<uint64_t>(masm()->heap())));
  __ movq(rsi, object);
  __ movq(rdx, property);
  __ movq(rcx, change);
  __ movq(rax, Immediate(*reinterpret_cast<uint64_t*>(&lookup)));
  __ callq(rax);

  __ Popad(rax);
  GenerateEpilogue(3);
}


void CoerceToBooleanStub::Generate() {
  GeneratePrologue();
  RuntimeCoerceCallback to_boolean = &RuntimeToBoolean;

  // Arguments
  Operand object(rbp, 16);

  __ Pushad();

  __ movq(rdi, Immediate(reinterpret_cast<uint64_t>(masm()->heap())));
  __ movq(rsi, object);
  __ movq(rax, Immediate(*reinterpret_cast<uint64_t*>(&to_boolean)));
  __ callq(rax);

  __ Popad(rax);

  __ CheckGC();

  GenerateEpilogue(1);
}


#define BINARY_SUB_TYPES(V)\
    V(Add)\
    V(Sub)\
    V(Mul)\
    V(Div)\
    V(BAnd)\
    V(BOr)\
    V(BXor)\
    V(Eq)\
    V(StrictEq)\
    V(Ne)\
    V(StrictNe)\
    V(Lt)\
    V(Gt)\
    V(Le)\
    V(Ge)\
    V(LOr)\
    V(LAnd)

#define BINARY_STUB_GENERATE(V)\
    void Binary##V##Stub::Generate() {\
      (new BinaryOpStub(masm(), BinOp::k##V))->Generate();\
    }
BINARY_SUB_TYPES(BINARY_STUB_GENERATE)
#undef BINARY_STUB_GENERATE


void BinaryOpStub::Generate() {
  GeneratePrologue();
  __ push(Immediate(0));
  __ push(rbx);

  // Arguments
  Operand lhs(rbp, 24);
  Operand rhs(rbp, 16);

  Label call_runtime(masm()), nil_result(masm()), done(masm());

  __ movq(rax, lhs);
  __ movq(rbx, rhs);

  if (BinOp::is_bool_logic(type())) {
    // Call runtime w/o any checks
    __ jmp(&call_runtime);
  }

  __ IsNil(rax, NULL, &call_runtime);
  __ IsNil(rbx, NULL, &call_runtime);

  __ IsHeapObject(Heap::kTagNumber, rax, &call_runtime, NULL);
  __ IsHeapObject(Heap::kTagNumber, rbx, &call_runtime, NULL);

  // We're adding two heap numbers
  Operand lvalue(rax, 8);
  Operand rvalue(rbx, 8);
  __ movq(rax, lvalue);
  __ movq(rbx, rvalue);
  __ movqd(xmm1, rax);
  __ movqd(xmm2, rbx);

  if (BinOp::is_math(type())) {
    switch (type()) {
     case BinOp::kAdd: __ addqd(xmm1, xmm2); break;
     case BinOp::kSub: __ subqd(xmm1, xmm2); break;
     case BinOp::kMul: __ mulqd(xmm1, xmm2); break;
     case BinOp::kDiv: __ divqd(xmm1, xmm2); break;
     default: __ emitb(0xcc); break;
    }

    __ AllocateNumber(xmm1, rax);
  } else if (BinOp::is_binary(type())) {
    // Truncate lhs and rhs first
    __ cvttsd2si(rax, xmm1);
    __ cvttsd2si(rbx, xmm2);

    switch (type()) {
     case BinOp::kBAnd: __ andq(rax, rbx); break;
     case BinOp::kBOr: __ orq(rax, rbx); break;
     case BinOp::kBXor: __ xorq(rax, rbx); break;
     default: __ emitb(0xcc); break;
    }

    __ TagNumber(rax);
  } else if (BinOp::is_logic(type())) {
    Condition cond = masm()->BinOpToCondition(type(), Masm::kDouble);
    __ ucomisd(xmm1, xmm2);

    Label true_(masm()), comp_end(masm());

    __ jmp(cond, &true_);

    __ movq(scratch, Immediate(masm()->TagNumber(0)));
    __ jmp(&comp_end);

    __ bind(&true_);
    __ movq(scratch, Immediate(masm()->TagNumber(1)));
    __ bind(&comp_end);

    __ AllocateBoolean(scratch, rax);
  } else if (BinOp::is_bool_logic(type())) {
    // Just call the runtime (see code above)
  }

  __ jmp(&done);
  __ bind(&call_runtime);

  RuntimeBinOpCallback cb;

#define BINARY_ENUM_CASES(V)\
    case BinOp::k##V: cb = &RuntimeBinOp<BinOp::k##V>; break;

  switch (type()) {
   BINARY_SUB_TYPES(BINARY_ENUM_CASES)
   default:
    UNEXPECTED
    break;
  }
#undef BINARY_ENUM_CASES

  {
    Label call(masm());

    Masm::Align a(masm());
    __ Pushad();

    Immediate heapref(reinterpret_cast<uint64_t>(masm()->heap()));

    // binop(heap, lhs, rhs)
    __ movq(rdi, heapref);
    __ movq(rsi, rax);
    __ movq(rdx, rbx);

    __ movq(scratch, Immediate(*reinterpret_cast<uint64_t*>(&cb)));
    __ callq(scratch);

    __ Popad(rax);
  }

  __ bind(&done);

  __ pop(rbx);

  __ CheckGC();

  // Caller should unwind stack
  GenerateEpilogue(0);
}

#undef BINARY_SUB_TYPES

} // namespace internal
} // namespace candor
