#include "stubs.h"
#include "code-space.h" // CodeSpace
#include "cpu.h" // CPU
#include "ast.h" // BinOp
#include "macroassembler.h" // Masm
#include "runtime.h"

namespace candor {
namespace internal {

#define __ masm()->

BaseStub::BaseStub(CodeSpace* space, StubType type) : space_(space),
                                                      masm_(space),
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
  __ push(r11);
  __ push(r12);
  __ push(r13);
  __ push(r14);
  __ push(r15);

  __ EnterFramePrologue();

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

  // Nullify rbp to help GC iterating frames
  __ xorq(rbp, rbp);

  // Nullify all registers to help GC distinguish on-stack values
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

  __ EnterFrameEpilogue();

  // Restore registers
  __ pop(r15);
  __ pop(r14);
  __ pop(r13);
  __ pop(r12);
  __ pop(r11);
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
  __ orqb(rbx, Immediate(0x01));

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

    __ Call(scratch);
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


void CallBindingStub::Generate() {
  GeneratePrologue();

  Operand argc(rbp, 24);
  Operand fn(rbp, 16);

  // Save all registers
  __ Pushad();

  // binding(argc, argv)
  __ movq(rdi, argc);
  __ Untag(rdi);
  __ movq(rsi, rbp);

  // old rbp + return address + two arguments
  __ addq(rsi, Immediate(4 * 8));

  // argv should point to the end of arguments array
  __ movq(scratch, rdi);
  __ shl(scratch, Immediate(3));
  __ addq(rsi, scratch);

  __ ExitFramePrologue();

  Operand code(scratch, 16);

  __ movq(scratch, fn);
  __ Call(code);

  __ ExitFrameEpilogue();

  // Restore all except rax
  __ Popad(rax);

  __ CheckGC();
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


void TypeofStub::Generate() {
  GeneratePrologue();

  Label not_nil(masm()), not_unboxed(masm()), done(masm());

  Operand type(rax, 0);

  __ IsNil(rax, &not_nil, NULL);

  __ movq(rax, Immediate(HContext::GetIndexDisp(Heap::kRootNilTypeIndex)));
  __ jmp(&done);
  __ bind(&not_nil);

  __ IsUnboxed(rax, &not_unboxed, NULL);
  __ movq(rax, Immediate(HContext::GetIndexDisp(Heap::kRootNumberTypeIndex)));

  __ jmp(&done);
  __ bind(&not_unboxed);

  Operand btag(rax, 0);
  __ movzxb(rax, btag);
  __ shl(rax, Immediate(3));
  __ addq(rax, Immediate(HContext::GetIndexDisp(
          Heap::kRootBooleanTypeIndex - Heap::kTagBoolean)));

  __ bind(&done);

  // rax contains offset in root_reg
  __ addq(rax, root_reg);
  __ movq(rax, type);

  GenerateEpilogue(0);
}


void SizeofStub::Generate() {
  GeneratePrologue();
  RuntimeSizeofCallback sizeofc = &RuntimeSizeof;

  __ Pushad();

  // RuntimeSizeof(heap, obj)
  __ movq(rdi, Immediate(reinterpret_cast<uint64_t>(masm()->heap())));
  __ movq(rsi, rax);
  __ movq(rax, Immediate(*reinterpret_cast<uint64_t*>(&sizeofc)));
  __ callq(rax);

  __ Popad(rax);

  GenerateEpilogue(0);
}


void KeysofStub::Generate() {
  GeneratePrologue();
  RuntimeKeysofCallback keysofc = &RuntimeKeysof;

  __ Pushad();

  // RuntimeKeysof(heap, obj)
  __ movq(rdi, Immediate(reinterpret_cast<uint64_t>(masm()->heap())));
  __ movq(rsi, rax);
  __ movq(rax, Immediate(*reinterpret_cast<uint64_t*>(&keysofc)));
  __ callq(rax);

  __ Popad(rax);

  GenerateEpilogue(0);
}


void LookupPropertyStub::Generate() {
  GeneratePrologue();
  __ AllocateSpills(0);

  Label is_object(masm()), is_array(masm()), cleanup(masm());
  Label non_object_error(masm()), done(masm());

  // rax <- object
  // rbx <- property
  // rcx <- change flag
  Masm::Spill object_s(masm(), rax);
  Masm::Spill change_s(masm(), rcx);

  // Return nil on non-object's property access
  __ IsNil(rax, NULL, &non_object_error);
  __ IsUnboxed(rax, NULL, &non_object_error);

  // Or into non-object
  __ IsHeapObject(Heap::kTagObject, rax, NULL, &is_object);
  __ IsHeapObject(Heap::kTagArray, rax, &non_object_error, &is_array);

  __ bind(&is_object);

  // Fast case object and a string key
  {
    __ IsNil(rbx, NULL, &is_array);
    __ IsUnboxed(rbx, NULL, &is_array);
    __ IsHeapObject(Heap::kTagString, rbx, &is_array, NULL);

    __ StringHash(rbx, rdx);

    Operand qmask(rax, HObject::kMaskOffset);
    __ movq(r15, qmask);

    // offset = hash & mask + kSpaceOffset
    __ andq(rdx, r15);
    __ addq(rdx, Immediate(HMap::kSpaceOffset));

    Operand qmap(rax, HObject::kMapOffset);
    __ movq(scratch, qmap);
    __ addq(scratch, rdx);

    Label match(masm());

    // rdx now contains pointer to the key slot in map's space
    // compare key's addresses
    Operand slot(scratch, 0);
    __ movq(scratch, slot);

    // Slot should contain either key
    __ cmpq(scratch, rbx);
    __ jmp(kEq, &match);

    // or nil
    __ cmpq(scratch, Immediate(Heap::kTagNil));
    __ jmp(kNe, &cleanup);

    __ bind(&match);

    Label fast_case_end(masm());

    // Insert key if was asked
    __ cmpq(rcx, Immediate(0));
    __ jmp(kEq, &fast_case_end);

    // Restore map's interior pointer
    __ movq(scratch, qmap);
    __ addq(scratch, rdx);

    // Put the key into slot
    __ movq(slot, rbx);

    __ bind(&fast_case_end);

    // Compute value's address
    // rax = key_offset + mask + 8
    __ movq(rax, rdx);
    __ addq(rax, r15);
    __ addq(rax, Immediate(8));

    // Cleanup
    __ xorq(r15, r15);
    __ xorq(rdx, rdx);

    // Return value
    GenerateEpilogue(0);
  }

  __ bind(&cleanup);

  __ xorq(r15, r15);
  __ xorq(rdx, rdx);

  __ bind(&is_array);

  __ Pushad();

  RuntimeLookupPropertyCallback lookup = &RuntimeLookupProperty;

  // RuntimeLookupProperty(heap, obj, key, change)
  // (returns addr of slot)
  __ movq(rdi, Immediate(reinterpret_cast<uint64_t>(masm()->heap())));
  __ movq(rsi, rax);
  __ movq(rdx, rbx);
  // rcx already contains change flag
  __ movq(rax, Immediate(*reinterpret_cast<uint64_t*>(&lookup)));
  __ callq(rax);

  __ Popad(rax);

  __ CheckGC();

  __ jmp(&done);

  __ bind(&non_object_error);

  // Non object lookups return nil
  __ movq(rax, Immediate(Heap::kTagNil));

  __ bind(&done);

  __ FinalizeSpills();
  GenerateEpilogue(0);
}


void CoerceToBooleanStub::Generate() {
  GeneratePrologue();

  Label unboxed(masm()), truel(masm()), not_bool(masm()), coerced_type(masm());

  // Check type and coerce if not boolean
  __ IsNil(rax, NULL, &not_bool);
  __ IsUnboxed(rax, NULL, &unboxed);
  __ IsHeapObject(Heap::kTagBoolean, rax, &not_bool, NULL);

  __ jmp(&coerced_type);

  __ bind(&unboxed);

  Operand truev(root_reg, HContext::GetIndexDisp(Heap::kRootTrueIndex));
  Operand falsev(root_reg, HContext::GetIndexDisp(Heap::kRootFalseIndex));

  __ cmpq(rax, Immediate(masm()->TagNumber(0)));
  __ jmp(kNe, &truel);

  __ movq(rax, falsev);

  __ jmp(&coerced_type);
  __ bind(&truel);

  __ movq(rax, truev);

  __ jmp(&coerced_type);
  __ bind(&not_bool);

  __ Pushad();

  RuntimeCoerceCallback to_boolean = &RuntimeToBoolean;

  __ movq(rdi, Immediate(reinterpret_cast<uint64_t>(masm()->heap())));
  __ movq(rsi, rax);
  __ movq(rax, Immediate(*reinterpret_cast<uint64_t*>(&to_boolean)));
  __ callq(rax);

  __ Popad(rax);

  __ bind(&coerced_type);

  __ CheckGC();

  GenerateEpilogue(0);
}


void CloneObjectStub::Generate() {
  GeneratePrologue();

  __ AllocateSpills(0);

  Label non_object(masm()), done(masm());

  // rax <- object
  __ IsNil(rax, NULL, &non_object);
  __ IsUnboxed(rax, NULL, &non_object);
  __ IsHeapObject(Heap::kTagObject, rax, &non_object, NULL);

  // Get map
  Operand qmap(rax, HObject::kMapOffset);
  __ movq(rax, qmap);

  // Get size
  Operand qsize(rax, HMap::kSizeOffset);
  __ movq(rcx, qsize);

  __ TagNumber(rcx);

  // Allocate new object
  __ AllocateObjectLiteral(Heap::kTagObject, rcx, rdx);

  __ movq(rbx, rdx);

  // Get new object's map
  qmap.base(rbx);
  __ movq(rbx, qmap);

  // Skip headers
  __ addq(rax, Immediate(HMap::kSpaceOffset));
  __ addq(rbx, Immediate(HMap::kSpaceOffset));

  // NOTE: rcx is tagged here

  // Copy all fields from it
  Label loop_start(masm()), loop_cond(masm());
  __ jmp(&loop_cond);
  __ bind(&loop_start);

  Operand from(rax, 0), to(rbx, 0);
  __ movq(scratch, from);
  __ movq(to, scratch);

  __ addq(rax, Immediate(8));
  __ addq(rbx, Immediate(8));

  __ dec(rcx);

  // Loop
  __ bind(&loop_cond);
  __ cmpq(rcx, Immediate(0));
  __ jmp(kNe, &loop_start);

  __ movq(rax, rdx);

  __ jmp(&done);
  __ bind(&non_object);

  // Non-object cloning - nil result
  __ movq(rax, Immediate(Heap::kTagNil));

  __ bind(&done);

  __ FinalizeSpills();

  GenerateEpilogue(0);
}


#define BINARY_SUB_TYPES(V)\
    V(Add)\
    V(Sub)\
    V(Mul)\
    V(Div)\
    V(Mod)\
    V(BAnd)\
    V(BOr)\
    V(BXor)\
    V(Shl)\
    V(Shr)\
    V(UShr)\
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

void BinOpStub::Generate() {
  GeneratePrologue();

  // rax <- lhs
  // rbx <- rhs

  // Allocate space for spill slots
  __ AllocateSpills(0);

  Label not_unboxed(masm()), done(masm());
  Label lhs_to_heap(masm()), rhs_to_heap(masm());

  if (type() != BinOp::kDiv) {
    // Try working with unboxed numbers

    __ IsUnboxed(rax, &not_unboxed, NULL);
    __ IsUnboxed(rbx, &not_unboxed, NULL);

    // Number (+) Number
    if (BinOp::is_math(type())) {
      Label restore(masm());
      Masm::Spill lvalue(masm(), rax);
      Masm::Spill rvalue(masm(), rbx);

      __ movq(scratch, rax);
      __ movq(rcx, rbx);

      switch (type()) {
       case BinOp::kAdd: __ addq(rax, rbx); break;
       case BinOp::kSub: __ subq(rax, rbx); break;
       case BinOp::kMul: __ Untag(rbx); __ imulq(rbx); break;

       default: __ emitb(0xcc); break;
      }

      // Call stub on overflow
      __ jmp(kNoOverflow, &done);

      // Restore numbers
      lvalue.Unspill();
      rvalue.Unspill();

      __ jmp(&not_unboxed);
    } else if (BinOp::is_binary(type())) {
      switch (type()) {
       case BinOp::kBAnd: __ andq(rax, rbx); break;
       case BinOp::kBOr: __ orq(rax, rbx); break;
       case BinOp::kBXor: __ xorq(rax, rbx); break;
       case BinOp::kMod:
        __ xorq(rdx, rdx);
        __ idivq(rbx);
        __ movq(rax, rdx);
        break;
       case BinOp::kShl:
       case BinOp::kShr:
       case BinOp::kUShr:
        __ movq(rcx, rbx);
        __ shr(rcx, Immediate(1));

        switch (type()) {
         case BinOp::kShl: __ sal(rax); break;
         case BinOp::kShr: __ sar(rax); break;
         case BinOp::kUShr: __ shr(rax); break;
         default: __ emitb(0xcc); break;
        }

        // Cleanup last bit
        __ shr(rax, Immediate(1));
        __ shl(rax, Immediate(1));

        break;

       default: __ emitb(0xcc); break;
      }
    } else if (BinOp::is_logic(type())) {
      Condition cond = masm()->BinOpToCondition(type(), Masm::kIntegral);
      // Note: rax and rbx are boxed here
      // Otherwise cmp won't work for negative numbers
      __ cmpq(rax, rbx);

      Label true_(masm()), cond_end(masm());

      Operand truev(root_reg, HContext::GetIndexDisp(Heap::kRootTrueIndex));
      Operand falsev(root_reg, HContext::GetIndexDisp(Heap::kRootFalseIndex));

      __ jmp(cond, &true_);

      __ movq(rax, falsev);
      __ jmp(&cond_end);

      __ bind(&true_);

      __ movq(rax, truev);
      __ bind(&cond_end);
    } else {
      // Call runtime for all other binary ops (boolean logic)
      __ jmp(&not_unboxed);
    }

    __ jmp(&done);
  }

  __ bind(&not_unboxed);

  Label box_rhs(masm()), both_boxed(masm());
  Label call_runtime(masm()), nil_result(masm());

  __ IsNil(rax, NULL, &call_runtime);
  __ IsNil(rbx, NULL, &call_runtime);

  // Convert lhs to heap number if needed
  __ IsUnboxed(rax, &box_rhs, NULL);

  __ Untag(rax);

  __ xorqd(xmm1, xmm1);
  __ cvtsi2sd(xmm1, rax);
  __ xorq(rax, rax);
  __ AllocateNumber(xmm1, rax);

  __ bind(&box_rhs);

  // Convert rhs to heap number if needed
  __ IsUnboxed(rbx, &both_boxed, NULL);

  __ Untag(rbx);

  __ xorqd(xmm1, xmm1);
  __ cvtsi2sd(xmm1, rbx);
  __ xorq(rbx, rbx);

  __ AllocateNumber(xmm1, rbx);

  // Both lhs and rhs are heap values (not-unboxed)
  __ bind(&both_boxed);

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
     case BinOp::kMod:
      __ xorq(rdx, rdx);
      __ idivq(rbx);
      __ movq(rax, rdx);
      break;
     case BinOp::kShl:
     case BinOp::kShr:
     case BinOp::kUShr:
      __ movq(rcx, rbx);

      switch (type()) {
       case BinOp::kUShr:
         __ shl(rax, Immediate(1));
         __ shr(rax);
         __ shr(rax, Immediate(1));
         break;
       case BinOp::kShl: __ shl(rax); break;
       case BinOp::kShr: __ shr(rax); break;
       default: __ emitb(0xcc); break;
      }
      break;
     default: __ emitb(0xcc); break;
    }

    __ TagNumber(rax);
  } else if (BinOp::is_logic(type())) {
    Condition cond = masm()->BinOpToCondition(type(), Masm::kDouble);
    __ ucomisd(xmm1, xmm2);

    Label true_(masm()), comp_end(masm());

    Operand truev(root_reg, HContext::GetIndexDisp(Heap::kRootTrueIndex));
    Operand falsev(root_reg, HContext::GetIndexDisp(Heap::kRootFalseIndex));

    __ jmp(cond, &true_);

    __ movq(rax, falsev);
    __ jmp(&comp_end);

    __ bind(&true_);
    __ movq(rax, truev);
    __ bind(&comp_end);
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

  Label call(masm());

  __ Pushad();

  Immediate heapref(reinterpret_cast<uint64_t>(masm()->heap()));

  // binop(heap, lhs, rhs)
  __ movq(rdi, heapref);
  __ movq(rsi, rax);
  __ movq(rdx, rbx);

  __ movq(scratch, Immediate(*reinterpret_cast<uint64_t*>(&cb)));
  __ callq(scratch);

  __ Popad(rax);

  __ bind(&done);

  __ CheckGC();

  __ FinalizeSpills();

  GenerateEpilogue(0);
}

#undef BINARY_SUB_TYPES

} // namespace internal
} // namespace candor
