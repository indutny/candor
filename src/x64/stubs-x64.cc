#include "stubs.h"
#include "code-space.h" // CodeSpace
#include "cpu.h" // CPU
#include "ast.h" // BinOp
#include "macroassembler.h" // Masm
#include "macroassembler-inl.h"
#include "runtime.h"
#include "pic.h"

namespace candor {
namespace internal {

#define __ masm()->

BaseStub::BaseStub(CodeSpace* space, StubType type) : space_(space),
                                                      masm_(space),
                                                      type_(type) {
}


void BaseStub::GeneratePrologue() {
  __ push(rbp);
  __ mov(rbp, rsp);
  __ AllocateSpills();
}


void BaseStub::GenerateEpilogue(int args) {
  __ FinalizeSpills();
  __ mov(rsp, rbp);
  __ pop(rbp);

  // tag + size
  __ ret(args * 8);
}


void EntryStub::Generate() {
  GeneratePrologue();

  // Just for alignment
  __ pushb(Immediate(Heap::kTagNil));

  // rdi <- function addr
  // rsi <- unboxed arguments count (tagged)
  // rdx <- pointer to arguments array

  // Store address of root context
  __ mov(root_reg, rdi);

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
  Label even, args, args_loop, unwind_even;
  __ mov(scratch, rsi);
  __ Untag(scratch);

  // Odd arguments count check (for alignment)
  __ testb(scratch, Immediate(1));
  __ jmp(kEq, &even);
  __ pushb(Immediate(0));
  __ bind(&even);

  // Get pointer to the end of arguments array
  __ mov(rbx, scratch);
  __ shl(rbx, Immediate(3));
  __ addq(rbx, rdx);

  __ jmp(&args_loop);

  __ bind(&args);

  __ subqb(rbx, Immediate(8));

  // Get argument from list
  Operand arg(rbx, 0);
  __ mov(rax, arg);
  __ push(rax);

  // Loop if needed
  __ bind(&args_loop);
  __ cmpq(rbx, rdx);
  __ jmp(kNe, &args);

  // Nullify all registers to help GC distinguish on-stack values
  __ xorq(rax, rax);
  __ xorq(rbx, rbx);
  __ xorq(rcx, rcx);
  __ xorq(rdx, rdx);
  // rsi, rdi <- context, root
  __ xorq(r8, r8);
  __ xorq(r9, r9);
  __ xorq(r10, r10);
  __ xorq(r11, r11);
  __ xorq(r12, r12);
  __ xorq(r13, r13);
  __ xorq(r14, r14);
  __ xorq(r15, r15);

  Masm::Spill rsi_s(masm(), rsi);

  // Put argc
  __ mov(rax, rsi);

  // Call code
  __ mov(scratch, rdi);
  __ CallFunction(scratch);

  // Unwind arguments
  rsi_s.Unspill();
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

  GenerateEpilogue(0);
}


void AllocateStub::Generate() {
  GeneratePrologue();
  // Align stack
  __ pushb(Immediate(0));
  __ push(rbx);

  // Arguments
  Operand size(rbp, 24);
  Operand tag(rbp, 16);

  Label runtime_allocate, done;

  Heap* heap = masm()->heap();
  Immediate heapref(reinterpret_cast<intptr_t>(heap));
  Immediate top(reinterpret_cast<intptr_t>(heap->new_space()->top()));
  Immediate limit(reinterpret_cast<intptr_t>(heap->new_space()->limit()));

  Operand scratch_op(scratch, 0);

  // Get pointer to current page's top
  // (new_space()->top() is a pointer to space's property
  // which is a pointer to page's top pointer
  // that's why we are dereferencing it here twice
  __ mov(scratch, top);
  __ mov(scratch, scratch_op);
  __ mov(rax, scratch_op);
  __ mov(rbx, size);
  __ Untag(rbx);

  // Add object size to the top
  __ addq(rbx, rax);
  __ jmp(kCarry, &runtime_allocate);

  // Check if we exhausted buffer
  __ mov(scratch, limit);
  __ mov(scratch, scratch_op);
  __ cmpq(rbx, scratch_op);
  __ jmp(kGt, &runtime_allocate);

  // We should allocate only even amount of bytes
  __ orqb(rbx, Immediate(0x01));

  // Update top
  __ mov(scratch, top);
  __ mov(scratch, scratch_op);
  __ mov(scratch_op, rbx);

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

    // Two arguments: heap, size
    __ mov(rdi, heapref);
    __ mov(rsi, size);

    __ mov(scratch, Immediate(*reinterpret_cast<intptr_t*>(&allocate)));

    __ Call(scratch);
    __ Popad(rax);
  }

  // Voila result and result_end are pointers
  __ bind(&done);

  // Set tag
  Operand qtag(rax, HValue::kTagOffset);
  __ mov(scratch, tag);
  __ Untag(scratch);
  __ mov(qtag, scratch);

  // Rax will hold resulting pointer
  __ pop(rbx);
  GenerateEpilogue(2);
}


void AllocateFunctionStub::Generate() {
  GeneratePrologue();

  // Arguments
  Operand argc(rbp, 24);
  Operand addr(rbp, 16);

  __ Allocate(Heap::kTagFunction, reg_nil, HValue::kPointerSize * 4, rax);

  // Move address of current context to first slot
  Operand qparent(rax, HFunction::kParentOffset);
  Operand qaddr(rax, HFunction::kCodeOffset);
  Operand qroot(rax, HFunction::kRootOffset);
  Operand qargc(rax, HFunction::kArgcOffset);

  __ mov(qparent, context_reg);
  __ mov(qroot, root_reg);

  // Put addr of code and argc
  __ mov(scratch, addr);
  __ mov(qaddr, scratch);
  __ mov(scratch, argc);
  __ mov(qargc, scratch);

  __ CheckGC();
  GenerateEpilogue(2);
}


void AllocateObjectStub::Generate() {
  GeneratePrologue();

  // Arguments
  Operand size(rbp, 24);
  Operand tag(rbp, 16);

  __ mov(rcx, tag);
  __ mov(rbx, size);
  __ AllocateObjectLiteral(Heap::kTagNil, rcx, rbx, rax);

  GenerateEpilogue(2);
}


void CallBindingStub::Generate() {
  GeneratePrologue();

  Operand argc(rbp, 24);
  Operand fn(rbp, 16);

  // Save all registers
  __ Pushad();

  // binding(argc, argv)
  __ mov(rdi, argc);
  __ Untag(rdi);
  __ mov(rsi, rbp);

  // old rbp + return address + two arguments
  __ addqb(rsi, Immediate(4 * 8));
  __ mov(scratch, rdi);
  __ shl(scratch, Immediate(3));
  __ subq(rsi, scratch);

  // argv should point to the end of arguments array
  __ mov(scratch, rdi);
  __ shl(scratch, Immediate(3));
  __ addq(rsi, scratch);

  __ ExitFramePrologue();

  Operand code(scratch, HFunction::kCodeOffset);

  __ mov(scratch, fn);
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
    __ mov(rdi, Immediate(reinterpret_cast<intptr_t>(masm()->heap())));
    __ mov(rsi, rsp);
    __ mov(rax, Immediate(*reinterpret_cast<intptr_t*>(&gc)));
    __ Call(rax);
  }

  __ Popad(reg_nil);

  GenerateEpilogue(0);
}


void TypeofStub::Generate() {
  GeneratePrologue();

  Label not_nil, not_unboxed, done;

  Operand type(rax, 0);

  __ IsNil(rax, &not_nil, NULL);

  __ mov(rax, Immediate(HContext::GetIndexDisp(Heap::kRootNilTypeIndex)));
  __ jmp(&done);
  __ bind(&not_nil);

  __ IsUnboxed(rax, &not_unboxed, NULL);
  __ mov(rax, Immediate(HContext::GetIndexDisp(Heap::kRootNumberTypeIndex)));

  __ jmp(&done);
  __ bind(&not_unboxed);

  Operand btag(rax, HValue::kTagOffset);
  __ movzxb(rax, btag);
  __ shl(rax, Immediate(3));
  __ addq(rax, Immediate(HContext::GetIndexDisp(
          Heap::kRootBooleanTypeIndex - Heap::kTagBoolean)));

  __ bind(&done);

  // rax contains offset in root_reg
  __ addq(rax, root_reg);
  __ mov(rax, type);

  GenerateEpilogue(0);
}


void SizeofStub::Generate() {
  GeneratePrologue();
  RuntimeSizeofCallback sizeofc = &RuntimeSizeof;

  __ Pushad();

  // RuntimeSizeof(heap, obj)
  __ mov(rdi, Immediate(reinterpret_cast<intptr_t>(masm()->heap())));
  __ mov(rsi, rax);
  __ mov(rax, Immediate(*reinterpret_cast<intptr_t*>(&sizeofc)));
  __ callq(rax);

  __ Popad(rax);

  GenerateEpilogue(0);
}


void KeysofStub::Generate() {
  GeneratePrologue();
  RuntimeKeysofCallback keysofc = &RuntimeKeysof;

  __ Pushad();

  // RuntimeKeysof(heap, obj)
  __ mov(rdi, Immediate(reinterpret_cast<intptr_t>(masm()->heap())));
  __ mov(rsi, rax);
  __ mov(rax, Immediate(*reinterpret_cast<intptr_t*>(&keysofc)));
  __ callq(rax);

  __ Popad(rax);

  GenerateEpilogue(0);
}


void LookupPropertyStub::Generate() {
  GeneratePrologue();

  Label is_object, is_array, cleanup, slow_case;
  Label non_object_error, done;

  // rax <- object
  // rbx <- property
  // rcx <- change flag
  Masm::Spill object_s(masm(), rax);
  Masm::Spill change_s(masm(), rcx);
  Masm::Spill rsi_s(masm(), rsi);

  // Return nil on non-object's property access
  __ IsUnboxed(rax, NULL, &non_object_error);
  __ IsNil(rax, NULL, &non_object_error);

  // Or into non-object
  __ IsHeapObject(Heap::kTagObject, rax, NULL, &is_object);
  __ IsHeapObject(Heap::kTagArray, rax, &non_object_error, &is_array);

  __ bind(&is_object);

  // Fast case: object and a string key
  {
    __ IsUnboxed(rbx, NULL, &slow_case);
    __ IsNil(rbx, NULL, &slow_case);
    __ IsHeapObject(Heap::kTagString, rbx, &slow_case, NULL);

    __ StringHash(rbx, rdx);

    Operand qmask(rax, HObject::kMaskOffset);
    __ mov(rsi, qmask);

    // offset = hash & mask + kSpaceOffset
    __ andq(rdx, rsi);
    __ addqb(rdx, Immediate(HMap::kSpaceOffset));

    Operand qmap(rax, HObject::kMapOffset);
    Operand qproto(rax, HObject::kProtoOffset);
    __ mov(scratch, qmap);
    __ addq(scratch, rdx);

    Label match, same_key;

    // rdx now contains pointer to the key slot in map's space
    // compare key's addresses
    Operand slot(scratch, 0);
    __ mov(scratch, slot);

    // Slot should contain either key
    __ cmpq(scratch, rbx);
    __ jmp(kEq, &match);

    // or nil
    __ cmpq(scratch, Immediate(Heap::kTagNil));
    __ jmp(kNe, &cleanup);

    __ bind(&match);

    Label fast_case_end;

    // Insert key if was asked
    __ cmpq(rcx, Immediate(0));
    __ jmp(kEq, &fast_case_end);

    // Inserting key in map is required
    // invalidate IC if key wasn't the same
    __ IsNil(scratch, &same_key, NULL);

    __ mov(qproto, Immediate(Heap::kICDisabledValue));
    __ bind(&same_key);

    // Restore map's interior pointer
    __ mov(scratch, qmap);
    __ addq(scratch, rdx);

    // Put the key into slot
    __ mov(slot, rbx);

    __ bind(&fast_case_end);

    // Compute value's address
    // rax = key_offset + mask + 8
    __ mov(rax, rdx);
    __ addq(rax, rsi);
    __ addqb(rax, Immediate(HValue::kPointerSize));

    // Cleanup
    __ xorq(rdx, rdx);
    rsi_s.Unspill();

    // Return value
    GenerateEpilogue(0);
  }

  __ bind(&is_array);
  // Fast case: dense array and a unboxed key
  {
    __ IsUnboxed(rbx, &slow_case, NULL);
    __ IsNil(rbx, NULL, &slow_case);
    __ cmpq(rbx, Immediate(-1));
    __ jmp(kLe, &slow_case);
    __ IsDenseArray(rax, &slow_case, NULL);

    // Get mask
    Operand qmask(rax, HObject::kMaskOffset);
    __ mov(rdx, qmask);

    // Check if index is above the mask
    // NOTE: rbx is tagged so we need to shift it only 2 times
    __ mov(rsi, rbx);
    __ shl(rsi, Immediate(2));
    __ cmpq(rsi, rdx);
    __ jmp(kGt, &cleanup);

    // Apply mask
    __ andq(rsi, rdx);

    // Check if length was increased
    Label length_set;

    Operand qlength(rax, HArray::kLengthOffset);
    __ mov(rdx, qlength);
    __ Untag(rbx);
    __ inc(rbx);
    __ cmpq(rbx, rdx);
    __ jmp(kLe, &length_set);

    // Update length
    __ mov(qlength, rbx);

    __ bind(&length_set);
    // Rbx is untagged here - so nullify it
    __ xorq(rbx, rbx);

    // Get index
    __ mov(rax, rsi);
    __ addqb(rax, Immediate(HMap::kSpaceOffset));

    // Cleanup
    __ xorq(rdx, rdx);
    rsi_s.Unspill();

    // Return value
    GenerateEpilogue(0);
  }

  __ bind(&cleanup);

  rsi_s.Unspill();
  __ xorq(rdx, rdx);

  __ bind(&slow_case);

  __ Pushad();

  RuntimeLookupPropertyCallback lookup = &RuntimeLookupProperty;

  // RuntimeLookupProperty(heap, obj, key, change)
  // (returns addr of slot)
  __ mov(rdi, Immediate(reinterpret_cast<intptr_t>(masm()->heap())));
  __ mov(rsi, rax);
  __ mov(rdx, rbx);
  // rcx already contains change flag
  __ mov(rax, Immediate(*reinterpret_cast<intptr_t*>(&lookup)));
  __ callq(rax);

  __ Popad(rax);

  __ jmp(&done);

  __ bind(&non_object_error);

  // Non object lookups return nil
  __ mov(rax, Immediate(Heap::kTagNil));

  __ bind(&done);

  GenerateEpilogue(0);
}


void PICMissStub::Generate() {
  GeneratePrologue();

  Operand self(rbp, 16);
  Operand object(rbp, 24);
  Operand result(rbp, 32);
  Operand ip(rbp, 40);

  // Amend PIC
  __ Pushad();

  __ mov(rdi, self);
  __ mov(rsi, object);
  __ mov(rdx, result);
  __ mov(rcx, ip);

  PIC::MissCallback miss_cb = &PIC::Miss;
  __ mov(rax, Immediate(*reinterpret_cast<intptr_t*>(&miss_cb)));
  __ Call(rax);

  __ Popad(reg_nil);

  GenerateEpilogue(0);
}


void CoerceToBooleanStub::Generate() {
  GeneratePrologue();

  Label unboxed, truel, not_bool, coerced_type;

  // Check type and coerce if not boolean
  __ IsUnboxed(rax, NULL, &unboxed);
  __ IsNil(rax, NULL, &not_bool);
  __ IsHeapObject(Heap::kTagBoolean, rax, &not_bool, NULL);

  __ jmp(&coerced_type);

  __ bind(&unboxed);

  Operand truev(root_reg, HContext::GetIndexDisp(Heap::kRootTrueIndex));
  Operand falsev(root_reg, HContext::GetIndexDisp(Heap::kRootFalseIndex));

  __ cmpq(rax, Immediate(HNumber::Tag(0)));
  __ jmp(kNe, &truel);

  __ mov(rax, falsev);

  __ jmp(&coerced_type);
  __ bind(&truel);

  __ mov(rax, truev);

  __ jmp(&coerced_type);
  __ bind(&not_bool);

  __ Pushad();

  RuntimeCoerceCallback to_boolean = &RuntimeToBoolean;

  __ mov(rdi, Immediate(reinterpret_cast<intptr_t>(masm()->heap())));
  __ mov(rsi, rax);
  __ mov(rax, Immediate(*reinterpret_cast<intptr_t*>(&to_boolean)));
  __ callq(rax);

  __ Popad(rax);

  __ bind(&coerced_type);

  __ CheckGC();

  GenerateEpilogue(0);
}


void CloneObjectStub::Generate() {
  GeneratePrologue();

  Label non_object, done;

  // rax <- object
  __ IsUnboxed(rax, NULL, &non_object);
  __ IsNil(rax, NULL, &non_object);
  __ IsHeapObject(Heap::kTagObject, rax, &non_object, NULL);

  // Get map
  Operand qmap(rax, HObject::kMapOffset);
  __ mov(rax, qmap);

  // Get size
  Operand qsize(rax, HMap::kSizeOffset);
  __ mov(rcx, qsize);

  __ TagNumber(rcx);

  // Allocate new object
  __ AllocateObjectLiteral(Heap::kTagObject, reg_nil, rcx, rdx);

  __ mov(rbx, rdx);

  // Get new object's map
  qmap.base(rbx);
  __ mov(rbx, qmap);

  // Set proto
  Operand qproto(rdx, HObject::kProtoOffset);
  __ mov(qproto, rax);

  // Skip headers
  __ addqb(rax, Immediate(HMap::kSpaceOffset));
  __ addqb(rbx, Immediate(HMap::kSpaceOffset));

  // NOTE: rcx is tagged here

  // Copy all fields from it
  Label loop_start, loop_cond;
  __ jmp(&loop_cond);
  __ bind(&loop_start);

  Operand from(rax, 0), to(rbx, 0);
  __ mov(scratch, from);
  __ mov(to, scratch);

  // Move forward
  __ addqb(rax, Immediate(8));
  __ addqb(rbx, Immediate(8));

  __ dec(rcx);

  // Loop
  __ bind(&loop_cond);
  __ cmpq(rcx, Immediate(0));
  __ jmp(kNe, &loop_start);

  __ mov(rax, rdx);

  __ jmp(&done);
  __ bind(&non_object);

  __ mov(rcx, Immediate(HNumber::Tag(16)));

  // Allocate new object
  __ AllocateObjectLiteral(Heap::kTagObject, reg_nil, rcx, rax);

  __ bind(&done);

  GenerateEpilogue(0);
}


void DeletePropertyStub::Generate() {
  GeneratePrologue();

  // rax <- receiver
  // rbx <- property
  //
  RuntimeDeletePropertyCallback delp = &RuntimeDeleteProperty;

  __ Pushad();

  // RuntimeDeleteProperty(heap, obj, property)
  __ mov(rdi, Immediate(reinterpret_cast<intptr_t>(masm()->heap())));
  __ mov(rsi, rax);
  __ mov(rdx, rbx);
  __ mov(rax, Immediate(*reinterpret_cast<intptr_t*>(&delp)));
  __ callq(rax);

  __ Popad(reg_nil);

  GenerateEpilogue(0);
}


void HashValueStub::Generate() {
  GeneratePrologue();

  Operand str(rbp, 16);

  RuntimeGetHashCallback hash = &RuntimeGetHash;

  __ Pushad();

  // RuntimeStringHash(heap, str)
  __ mov(rdi, Immediate(reinterpret_cast<intptr_t>(masm()->heap())));
  __ mov(rsi, str);
  __ mov(rax, Immediate(*reinterpret_cast<intptr_t*>(&hash)));
  __ callq(rax);

  __ Popad(rax);

  // Caller will unwind stack
  GenerateEpilogue(0);
}


void StackTraceStub::Generate() {
  // Store caller's frame pointer
  __ mov(rbx, rbp);

  GeneratePrologue();

  // rax <- ip
  // rbx <- rbp
  //
  RuntimeStackTraceCallback strace = &RuntimeStackTrace;

  __ Pushad();

  // RuntimeStackTrace(heap, frame, ip)
  __ mov(rdi, Immediate(reinterpret_cast<intptr_t>(masm()->heap())));
  __ mov(rsi, rbx);
  __ mov(rdx, rax);

  __ mov(rax, Immediate(*reinterpret_cast<intptr_t*>(&strace)));
  __ callq(rax);

  __ Popad(rax);

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

  Label not_unboxed, done;
  Label lhs_to_heap, rhs_to_heap;

  if (type() != BinOp::kDiv) {
    // Try working with unboxed numbers

    __ IsUnboxed(rax, &not_unboxed, NULL);
    __ IsUnboxed(rbx, &not_unboxed, NULL);

    // Number (+) Number
    if (BinOp::is_math(type())) {
      Masm::Spill lvalue(masm(), rax);
      Masm::Spill rvalue(masm(), rbx);

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
        {
          Label zero;
          __ xorq(rdx, rdx);
          __ cmpq(rbx, Immediate(HNumber::Tag(0)));
          __ jmp(kEq, &zero);
          __ idivq(rbx);
          __ bind(&zero);
          __ mov(rax, rdx);
        }
        break;
       case BinOp::kShl:
       case BinOp::kShr:
       case BinOp::kUShr:
        __ mov(rcx, rbx);
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

      Label true_, cond_end;

      Operand truev(root_reg, HContext::GetIndexDisp(Heap::kRootTrueIndex));
      Operand falsev(root_reg, HContext::GetIndexDisp(Heap::kRootFalseIndex));

      __ jmp(cond, &true_);

      __ mov(rax, falsev);
      __ jmp(&cond_end);

      __ bind(&true_);

      __ mov(rax, truev);
      __ bind(&cond_end);
    } else {
      // Call runtime for all other binary ops (boolean logic)
      __ jmp(&not_unboxed);
    }

    __ jmp(&done);
  }

  __ bind(&not_unboxed);

  Label box_rhs, both_boxed;
  Label call_runtime, nil_result;

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
  Operand lvalue(rax, HNumber::kValueOffset);
  Operand rvalue(rbx, HNumber::kValueOffset);
  __ movd(xmm1, lvalue);
  __ movd(xmm2, rvalue);
  __ xorq(rbx, rbx);

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
      {
        Label zero;
        __ xorq(rdx, rdx);
        __ cmpq(rbx, Immediate(HNumber::Tag(0)));
        __ jmp(kEq, &zero);
        __ idivq(rbx);
        __ bind(&zero);
        __ mov(rax, rdx);
      }
      break;
     case BinOp::kShl:
     case BinOp::kShr:
     case BinOp::kUShr:
      __ mov(rcx, rbx);

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

    Label true_, comp_end;

    Operand truev(root_reg, HContext::GetIndexDisp(Heap::kRootTrueIndex));
    Operand falsev(root_reg, HContext::GetIndexDisp(Heap::kRootFalseIndex));

    __ jmp(cond, &true_);

    __ mov(rax, falsev);
    __ jmp(&comp_end);

    __ bind(&true_);
    __ mov(rax, truev);
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

  Label call;

  __ Pushad();

  Immediate heapref(reinterpret_cast<intptr_t>(masm()->heap()));

  // binop(heap, lhs, rhs)
  __ mov(rdi, heapref);
  __ mov(rsi, rax);
  __ mov(rdx, rbx);

  __ mov(scratch, Immediate(*reinterpret_cast<intptr_t*>(&cb)));
  __ callq(scratch);

  __ Popad(rax);

  __ bind(&done);

  // Cleanup
  __ xorq(rdx, rdx);
  __ xorq(rcx, rcx);
  __ xorq(rbx, rbx);

  __ CheckGC();

  GenerateEpilogue(0);
}

#undef BINARY_SUB_TYPES


void LoadVarArgStub::Generate() {
  __ mov(rdx, rbp);
  GeneratePrologue();

  // offset and rest are unboxed
  Register offset = rax;
  Register rest = rbx;
  Register arr = rcx;
  Masm::Spill rbp_s(masm(), rdx);
  Operand argc(rdx, -HValue::kPointerSize * 2);
  Operand qmap(arr, HObject::kMapOffset);
  Operand slot(scratch, 0);
  Operand stack_slot(offset, 0);

  Label loop, preloop, end;

  // Calculate length of vararg array
  __ mov(scratch, offset);
  __ addq(scratch, rest);

  // If offset + rest <= argc - return immediately
  __ cmpq(scratch, argc);
  __ jmp(kGe, &end);

  // rdx = argc - offset - rest
  __ mov(rdx, argc);
  __ subq(rdx, scratch);

  // Array index
  __ mov(rbx, Immediate(HNumber::Tag(0)));

  Masm::Spill arr_s(masm(), arr), rdx_s(masm());
  Masm::Spill offset_s(masm(), offset), rbx_s(masm());

  __ bind(&loop);

  // while (rdx > 0)
  __ cmpq(rdx, Immediate(HNumber::Tag(0)));
  __ jmp(kEq, &end);

  rdx_s.SpillReg(rdx);
  rbx_s.SpillReg(rbx);

  __ mov(rax, arr);

  // rax <- object
  // rbx <- property
  __ mov(rcx, Immediate(1));
  __ Call(masm()->stubs()->GetLookupPropertyStub());

  arr_s.Unspill();
  rbx_s.Unspill();

  // Make rax look like unboxed number to GC
  __ dec(rax);
  __ CheckGC();
  __ inc(rax);

  __ IsNil(rax, NULL, &preloop);

  __ mov(arr, qmap);
  __ addq(rax, arr);
  __ mov(scratch, rax);

  // Get stack offset
  offset_s.Unspill();
  __ addqb(offset, Immediate(HNumber::Tag(2)));
  __ addq(offset, rbx);
  __ shl(offset, 2);
  __ addq(offset, *rbp_s.GetOperand());
  __ mov(offset, stack_slot);

  // Put argument in array
  __ mov(slot, offset);

  arr_s.Unspill();

  __ bind(&preloop);

  // Increment array index
  __ addqb(rbx, Immediate(HNumber::Tag(1)));

  // rdx --
  rdx_s.Unspill();
  __ subqb(rdx, Immediate(HNumber::Tag(1)));
  __ jmp(&loop);

  __ bind(&end);

  // Cleanup?
  __ xorq(rax, rax);
  __ xorq(rbx, rbx);
  __ xorq(rdx, rdx);
  // rcx <- holds result

  GenerateEpilogue(0);
}


void StoreVarArgStub::Generate() {
  GeneratePrologue();

  Register varg = rax;
  Register index = rbx;
  Register map = rcx;
  Register stack = rdx;
  Operand stack_slot(stack, 0);

  // rax <- varg
  Label loop, not_array, odd_end, r1_nil, r2_nil;
  Masm::Spill index_s(masm()), map_s(masm()), array_s(masm()), r1(masm());
  Masm::Spill stack_s(masm(), stack);
  Operand slot(rax, 0);

  __ IsUnboxed(varg, NULL, &not_array);
  __ IsNil(varg, NULL, &not_array);
  __ IsHeapObject(Heap::kTagArray, varg, &not_array, NULL);

  Operand qmap(varg, HObject::kMapOffset);
  __ mov(map, qmap);
  map_s.SpillReg(map);

  // index = sizeof(array)
  Operand qlength(varg, HArray::kLengthOffset);
  __ mov(index, qlength);
  __ TagNumber(index);

  // while ...
  __ bind(&loop);

  array_s.SpillReg(varg);

  // while ... (index != 0) {
  __ cmpq(index, Immediate(HNumber::Tag(0)));
  __ jmp(kEq, &not_array);

  // index--;
  __ subqb(index, Immediate(HNumber::Tag(1)));

  index_s.SpillReg(index);

  // odd case: array[index]
  __ mov(rbx, index);
  __ mov(rcx, Immediate(0));
  __ Call(masm()->stubs()->GetLookupPropertyStub());

  __ IsNil(rax, NULL, &r1_nil);
  map_s.Unspill();
  __ addq(rax, map);
  __ mov(rax, slot);

  __ bind(&r1_nil);
  r1.SpillReg(rax);

  index_s.Unspill();

  // if (index == 0) goto odd_end;
  __ cmpq(index, Immediate(HNumber::Tag(0)));
  __ jmp(kEq, &odd_end);

  // index--;
  __ subqb(index, Immediate(HNumber::Tag(1)));

  array_s.Unspill();
  index_s.SpillReg(index);

  // even case: array[index]
  __ mov(rbx, index);
  __ mov(rcx, Immediate(0));
  __ Call(masm()->stubs()->GetLookupPropertyStub());

  __ IsNil(rax, NULL, &r2_nil);
  map_s.Unspill();
  __ addq(rax, map);
  __ mov(rax, slot);

  __ bind(&r2_nil);

  // Push two item at the same time (to preserve alignment)
  r1.Unspill(index);
  stack_s.Unspill();
  __ mov(stack_slot, index);
  __ subqb(stack, Immediate(8));
  __ mov(stack_slot, rax);
  __ subqb(stack, Immediate(8));
  stack_s.SpillReg(stack);

  index_s.Unspill();
  array_s.Unspill();

  __ jmp(&loop);

  // }
  __ bind(&odd_end);

  r1.Unspill(rax);
  stack_s.Unspill();
  __ mov(stack_slot, rax);
  __ subqb(stack, Immediate(8));
  stack_s.SpillReg(stack);

  __ bind(&not_array);

  __ xorl(map, map);
  GenerateEpilogue(0);
}

} // namespace internal
} // namespace candor
