#include "stubs.h"
#include "code-space.h" // CodeSpace
#include "cpu.h" // CPU
#include "ast.h" // BinOp
#include "macroassembler.h" // Masm
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
  __ push(ebp);
  __ mov(ebp, esp);
  __ AllocateSpills();
}


void BaseStub::GenerateEpilogue(int args) {
  __ FinalizeSpills();
  __ mov(esp, ebp);
  __ pop(ebp);

  // Caller should unwind stack
  __ ret(0);
}


void EntryStub::Generate() {
  GeneratePrologue();

  // Store callee-save registers
  __ push(ebx);
  __ push(esi);
  __ push(edi);
  __ push(edi);

  Operand fn(ebp, 2 * 4);
  Operand argc(ebp, 3 * 4);
  Operand argv(ebp, 4 * 4);

  __ EnterFramePrologue();

  // Push all arguments to stack
  Label even, align1, align2, align3, args, args_loop, unwind_even;
  __ mov(eax, argc);
  __ Untag(eax);

  // Odd arguments count check (for alignment)
  // NOTE: On ia32 we're trying to make argc % 4 == 0
  __ mov(ebx, eax);
  __ andb(ebx, Immediate(3));

  __ cmpb(ebx, Immediate(0));
  __ jmp(kEq, &even);
  __ cmpb(ebx, Immediate(1));
  __ jmp(kEq, &align1);
  __ cmpb(ebx, Immediate(2));
  __ jmp(kEq, &align2);
  __ cmpb(ebx, Immediate(3));
  __ jmp(kEq, &align3);

  __ bind(&align1);
  __ pushb(Immediate(0));
  __ bind(&align2);
  __ pushb(Immediate(0));
  __ bind(&align3);
  __ pushb(Immediate(0));

  __ bind(&even);

  // Get pointer to the end of arguments array
  __ mov(ebx, eax);
  __ shl(ebx, Immediate(2));
  __ mov(edx, argv);
  __ addl(ebx, edx);

  __ jmp(&args_loop);

  __ bind(&args);

  __ sublb(ebx, Immediate(4));

  // Get argument from list
  Operand arg(ebx, 0);
  __ mov(eax, arg);
  __ push(eax);

  // Loop if needed
  __ bind(&args_loop);
  __ cmpl(ebx, edx);
  __ jmp(kNe, &args);

  // Nullify all registers to help GC distinguish on-stack values
  __ xorl(ebx, ebx);
  __ xorl(ecx, ecx);
  __ xorl(edx, edx);
  __ xorl(scratch, scratch);

  // Put argc
  __ mov(eax, argc);

  // Call code
  __ mov(ebx, fn);
  __ CallFunction(ebx);

  // Unwind arguments
  __ mov(ebx, argc);
  __ Untag(ebx);

  __ testb(ebx, Immediate(3));
  __ jmp(kEq, &unwind_even);
  __ orlb(ebx, Immediate(3));
  __ inc(ebx);
  __ bind(&unwind_even);

  __ shl(ebx, Immediate(2));
  __ addl(esp, ebx);

  __ EnterFrameEpilogue();

  // Restore callee-save registers
  __ pop(edi);
  __ pop(edi);
  __ pop(esi);
  __ pop(ebx);

  GenerateEpilogue();
}


void AllocateStub::Generate() {
  GeneratePrologue();

  // Align stack
  __ pushb(Immediate(0));
  __ pushb(Immediate(0));
  __ pushb(Immediate(0));
  __ push(ebx);

  // Arguments
  Operand size(ebp, 3 * 4);
  Operand tag(ebp, 2 * 4);

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
  __ mov(eax, scratch_op);
  __ mov(ebx, size);
  __ Untag(ebx);

  // Add object size to the top
  __ addl(ebx, eax);
  __ jmp(kCarry, &runtime_allocate);

  // Check if we exhausted buffer
  __ mov(scratch, limit);
  __ mov(scratch, scratch_op);
  __ cmpl(ebx, scratch_op);
  __ jmp(kGt, &runtime_allocate);

  // We should allocate only even amount of bytes
  __ orlb(ebx, Immediate(0x01));

  // Update top
  __ mov(scratch, top);
  __ mov(scratch, scratch_op);
  __ mov(scratch_op, ebx);

  __ jmp(&done);

  // Invoke runtime allocation stub
  __ bind(&runtime_allocate);

  // Remove junk from registers
  __ xorl(eax, eax);
  __ xorl(ebx, ebx);

  RuntimeAllocateCallback allocate = &RuntimeAllocate;

  {
    Masm::Align a(masm());
    __ Pushad();

    __ mov(scratch, size);
    __ push(scratch);
    __ push(scratch);

    // Two arguments: heap, size
    __ push(scratch);
    __ push(heapref);
    __ mov(scratch, Immediate(*reinterpret_cast<intptr_t*>(&allocate)));

    __ Call(scratch);
    __ addlb(esp, 4 * 4);
    __ Popad(eax);
  }

  // Voila result and result_end are pointers
  __ bind(&done);

  // Set tag
  Operand qtag(eax, HValue::kTagOffset);
  __ mov(scratch, tag);
  __ Untag(scratch);
  __ mov(qtag, scratch);

  // eax will hold resulting pointer
  __ pop(ebx);
  GenerateEpilogue();
}


void AllocateFunctionStub::Generate() {
  GeneratePrologue();

  // Arguments
  Operand argc(ebp, 3 * 4);
  Operand addr(ebp, 2 * 4);

  __ Allocate(Heap::kTagFunction, reg_nil, HValue::kPointerSize * 4, eax);

  // Move address of current context to first slot
  Operand qparent(eax, HFunction::kParentOffset);
  Operand qaddr(eax, HFunction::kCodeOffset);
  Operand qroot(eax, HFunction::kRootOffset);
  Operand qargc(eax, HFunction::kArgcOffset);
  Heap* heap = masm()->heap();
  Immediate root(reinterpret_cast<intptr_t>(heap->old_space()->root()));
  Operand scratch_op(scratch, 0);

  __ mov(qparent, context_reg);
  __ mov(scratch, root);
  __ mov(scratch, scratch_op);
  __ mov(qroot, scratch);

  // Put addr of code and argc
  __ mov(scratch, addr);
  __ mov(qaddr, scratch);
  __ mov(scratch, argc);
  __ mov(qargc, scratch);

  __ CheckGC();
  GenerateEpilogue();
}


void AllocateObjectStub::Generate() {
  GeneratePrologue();

  // Arguments
  Operand size(ebp, 3 * 4);
  Operand tag(ebp, 2 * 4);

  __ mov(ecx, tag);
  __ mov(ebx, size);
  __ AllocateObjectLiteral(Heap::kTagNil, ecx, ebx, eax);

  GenerateEpilogue();
}


void CallBindingStub::Generate() {
  GeneratePrologue();

  Operand argc(ebp, 3 * 4);
  Operand fn(ebp, 2 * 4);

  // Save all registers
  __ Pushad();

  // binding(argc, argv)
  __ mov(eax, argc);
  __ Untag(eax);
  __ mov(ebx, ebp);

  // old ebp + return address + two arguments + two words alignment
  __ addlb(ebx, Immediate(6 * 4));
  __ mov(scratch, eax);
  __ shl(scratch, Immediate(2));
  __ subl(ebx, scratch);

  // argv should point to the end of arguments array
  __ mov(scratch, eax);
  __ shl(scratch, Immediate(2));
  __ addl(ebx, scratch);

  __ ExitFramePrologue();

  Operand code(scratch, HFunction::kCodeOffset);

  __ push(ebx); // align
  __ push(ebx); //
  __ push(ebx); // <- argv
  __ push(eax); // <- argc
  __ mov(scratch, fn);
  __ Call(code);
  __ addlb(esp, Immediate(4 * 4));

  __ ExitFrameEpilogue();

  // Restore all except eax
  __ Popad(eax);

  __ CheckGC();
  GenerateEpilogue();
}


void CollectGarbageStub::Generate() {
  GeneratePrologue();

  RuntimeCollectGarbageCallback gc = &RuntimeCollectGarbage;
  __ Pushad();

  {
    Masm::Align a(masm());

    // RuntimeCollectGarbage(heap, stack_top)
    __ mov(edi, Immediate(reinterpret_cast<intptr_t>(masm()->heap())));
    __ mov(esi, esp);

    __ push(esi);
    __ push(esi);

    __ push(esi);
    __ push(edi);

    __ mov(eax, Immediate(*reinterpret_cast<intptr_t*>(&gc)));
    __ Call(eax);

    __ addlb(esp, Immediate(4 * 4));
  }

  __ Popad(reg_nil);

  GenerateEpilogue();
}


void TypeofStub::Generate() {
  GeneratePrologue();
  Heap* heap = masm()->heap();
  Immediate root(reinterpret_cast<intptr_t>(heap->old_space()->root()));
  Operand scratch_op(scratch, 0);

  Label not_nil, not_unboxed, done;

  Operand type(eax, 0);

  __ IsNil(eax, &not_nil, NULL);

  __ mov(eax, Immediate(HContext::GetIndexDisp(Heap::kRootNilTypeIndex)));
  __ jmp(&done);
  __ bind(&not_nil);

  __ IsUnboxed(eax, &not_unboxed, NULL);
  __ mov(eax, Immediate(HContext::GetIndexDisp(Heap::kRootNumberTypeIndex)));

  __ jmp(&done);
  __ bind(&not_unboxed);

  Operand btag(eax, HValue::kTagOffset);
  __ movzxb(eax, btag);
  __ shl(eax, Immediate(2));
  __ addl(eax, Immediate(HContext::GetIndexDisp(
          Heap::kRootBooleanTypeIndex - Heap::kTagBoolean)));

  __ bind(&done);

  // eax contains offset in root_reg
  __ mov(scratch, root);
  __ mov(scratch, scratch_op);
  __ addl(eax, scratch);
  __ mov(eax, type);

  GenerateEpilogue();
}


void SizeofStub::Generate() {
  GeneratePrologue();
  RuntimeSizeofCallback sizeofc = &RuntimeSizeof;

  __ Pushad();

  // RuntimeSizeof(heap, obj)
  __ mov(edi, Immediate(reinterpret_cast<intptr_t>(masm()->heap())));
  __ mov(esi, eax);
  __ mov(eax, Immediate(*reinterpret_cast<intptr_t*>(&sizeofc)));

  __ push(esi);
  __ push(esi);

  __ push(esi);
  __ push(edi);
  __ call(eax);
  __ addlb(esp, Immediate(4 * 4));

  __ Popad(eax);

  GenerateEpilogue(0);
}


void KeysofStub::Generate() {
  GeneratePrologue();

  RuntimeKeysofCallback keysofc = &RuntimeKeysof;

  __ Pushad();

  // RuntimeKeysof(heap, obj)
  __ mov(edi, Immediate(reinterpret_cast<intptr_t>(masm()->heap())));
  __ mov(esi, eax);
  __ mov(eax, Immediate(*reinterpret_cast<intptr_t*>(&keysofc)));

  __ push(esi);
  __ push(esi);

  __ push(esi);
  __ push(edi);
  __ call(eax);
  __ addlb(esp, Immediate(4 * 4));

  __ Popad(eax);

  GenerateEpilogue(0);
}


void LookupPropertyStub::Generate() {
  GeneratePrologue();

  Label is_object, is_array, cleanup, slow_case;
  Label non_object_error, done;

  // eax <- object
  // ebx <- property
  // ecx <- change flag
  Masm::Spill object_s(masm(), eax);
  Masm::Spill change_s(masm(), ecx);
  Masm::Spill esi_s(masm(), esi);

  // Return nil on non-object's property access
  __ IsUnboxed(eax, NULL, &non_object_error);
  __ IsNil(eax, NULL, &non_object_error);

  // Or into non-object
  __ IsHeapObject(Heap::kTagObject, eax, NULL, &is_object);
  __ IsHeapObject(Heap::kTagArray, eax, &non_object_error, &is_array);

  __ bind(&is_object);

  // Fast case: object and a string key
  {
    __ IsUnboxed(ebx, NULL, &slow_case);
    __ IsNil(ebx, NULL, &slow_case);
    __ IsHeapObject(Heap::kTagString, ebx, &slow_case, NULL);

    __ StringHash(ebx, edx);

    Operand qmask(eax, HObject::kMaskOffset);
    __ mov(esi, qmask);

    // offset = hash & mask + kSpaceOffset
    __ andl(edx, esi);
    __ addlb(edx, Immediate(HMap::kSpaceOffset));

    Operand qmap(eax, HObject::kMapOffset);
    Operand qproto(eax, HObject::kProtoOffset);
    __ mov(scratch, qmap);
    __ addl(scratch, edx);

    Label match;

    // edx now contains pointer to the key slot in map's space
    // compare key's addresses
    Operand slot(scratch, 0);
    __ mov(scratch, slot);

    // Slot should contain either key
    __ cmpl(scratch, ebx);
    __ jmp(kEq, &match);

    // or nil
    __ cmpl(scratch, Immediate(Heap::kTagNil));
    __ jmp(kNe, &cleanup);

    __ bind(&match);

    Label fast_case_end, same_key;

    // Insert key if was asked
    __ cmpl(ecx, Immediate(0));
    __ jmp(kEq, &fast_case_end);

    // Inserting key in map is required
    // invalidate IC if key wasn't the same
    __ IsNil(scratch, &same_key, NULL);

    __ mov(qproto, Immediate(Heap::kICDisabledValue));
    __ bind(&same_key);

    // Restore map's interior pointer
    __ mov(scratch, qmap);
    __ addl(scratch, edx);

    // Put the key into slot
    __ mov(slot, ebx);

    __ bind(&fast_case_end);

    // Compute value's address
    // eax = key_offset + mask + 4
    __ mov(eax, edx);
    __ addl(eax, esi);
    __ addlb(eax, Immediate(HValue::kPointerSize));

    // Cleanup
    __ xorl(edx, edx);
    esi_s.Unspill();

    // Return value
    GenerateEpilogue(0);
  }

  __ bind(&is_array);
  // Fast case: dense array and a unboxed key
  {
    __ IsUnboxed(ebx, &slow_case, NULL);
    __ IsNil(ebx, NULL, &slow_case);
    __ cmpl(ebx, Immediate(-1));
    __ jmp(kLe, &slow_case);
    __ IsDenseArray(eax, &slow_case, NULL);

    // Get mask
    Operand qmask(eax, HObject::kMaskOffset);
    __ mov(edx, qmask);

    // Check if index is above the mask
    // NOTE: ebx is tagged so we need to shift it only 1 time
    __ mov(esi, ebx);
    __ shl(esi, Immediate(1));
    __ cmpl(esi, edx);
    __ jmp(kGt, &cleanup);

    // Apply mask
    __ andl(esi, edx);

    // Check if length was increased
    Label length_set;

    Operand qlength(eax, HArray::kLengthOffset);
    __ mov(edx, qlength);
    __ Untag(ebx);
    __ inc(ebx);
    __ cmpl(ebx, edx);
    __ jmp(kLe, &length_set);

    // Update length
    __ mov(qlength, ebx);

    __ bind(&length_set);
    // ebx is untagged here - so nullify it
    __ xorl(ebx, ebx);

    // Get index
    __ mov(eax, esi);
    __ addlb(eax, Immediate(HMap::kSpaceOffset));

    // Cleanup
    __ xorl(edx, edx);
    esi_s.Unspill();

    // Return value
    GenerateEpilogue(0);
  }

  __ bind(&cleanup);

  esi_s.Unspill();
  __ xorl(edx, edx);

  __ bind(&slow_case);

  __ Pushad();

  RuntimeLookupPropertyCallback lookup = &RuntimeLookupProperty;

  // RuntimeLookupProperty(heap, obj, key, change)
  // (returns addr of slot)
  __ mov(edi, Immediate(reinterpret_cast<intptr_t>(masm()->heap())));
  __ mov(esi, eax);
  __ mov(edx, ebx);
  // ecx already contains change flag
  __ mov(eax, Immediate(*reinterpret_cast<intptr_t*>(&lookup)));

  __ push(ecx);
  __ push(edx);
  __ push(esi);
  __ push(edi);
  __ call(eax);
  __ addlb(esp, Immediate(4 * 4));

  __ Popad(eax);

  __ jmp(&done);

  __ bind(&non_object_error);

  // Non object lookups return nil
  __ mov(eax, Immediate(Heap::kTagNil));

  __ bind(&done);

  GenerateEpilogue();
}


void PICMissStub::Generate() {
  GeneratePrologue();

  Operand space(ebp, 8);
  Operand object(ebp, 12);
  Operand result(ebp, 16);
  Operand ip(ebp, 20);

  // Amend PIC
  __ Pushad();

  __ push(ip);
  __ push(result);
  __ push(object);
  __ push(space);

  PIC::MissCallback miss_cb = &PIC::Miss;
  __ mov(scratch, Immediate(*reinterpret_cast<intptr_t*>(&miss_cb)));
  __ Call(scratch);
  __ addlb(esp, Immediate(4 * 4));

  __ Popad(reg_nil);

  GenerateEpilogue(0);
}


void CoerceToBooleanStub::Generate() {
  GeneratePrologue();
  Heap* heap = masm()->heap();
  Immediate root(reinterpret_cast<intptr_t>(heap->old_space()->root()));
  Operand scratch_op(scratch, 0);

  Label unboxed, truel, not_bool, coerced_type;

  // Check type and coerce if not boolean
  __ IsUnboxed(eax, NULL, &unboxed);
  __ IsNil(eax, NULL, &not_bool);
  __ IsHeapObject(Heap::kTagBoolean, eax, &not_bool, NULL);

  __ jmp(&coerced_type);

  __ bind(&unboxed);

  __ mov(scratch, root);
  __ mov(scratch, scratch_op);
  Operand truev(scratch, HContext::GetIndexDisp(Heap::kRootTrueIndex));
  Operand falsev(scratch, HContext::GetIndexDisp(Heap::kRootFalseIndex));

  __ cmpl(eax, Immediate(HNumber::Tag(0)));
  __ jmp(kNe, &truel);

  __ mov(eax, falsev);

  __ jmp(&coerced_type);
  __ bind(&truel);

  __ mov(eax, truev);

  __ jmp(&coerced_type);
  __ bind(&not_bool);

  __ Pushad();

  RuntimeCoerceCallback to_boolean = &RuntimeToBoolean;

  __ mov(edi, Immediate(reinterpret_cast<intptr_t>(masm()->heap())));
  __ mov(esi, eax);
  __ mov(eax, Immediate(*reinterpret_cast<intptr_t*>(&to_boolean)));

  __ push(esi);
  __ push(esi);

  __ push(esi);
  __ push(edi);
  __ call(eax);
  __ addlb(esp, Immediate(4 * 4));

  __ Popad(eax);

  __ bind(&coerced_type);

  __ CheckGC();

  GenerateEpilogue(0);
}


void CloneObjectStub::Generate() {
  GeneratePrologue();

  Label non_object, done;

  // eax <- object
  __ IsUnboxed(eax, NULL, &non_object);
  __ IsNil(eax, NULL, &non_object);
  __ IsHeapObject(Heap::kTagObject, eax, &non_object, NULL);

  // Get map
  Operand qmap(eax, HObject::kMapOffset);
  __ mov(eax, qmap);

  // Get size
  Operand qsize(eax, HMap::kSizeOffset);
  __ mov(ecx, qsize);

  __ TagNumber(ecx);

  // Allocate new object
  __ AllocateObjectLiteral(Heap::kTagObject, reg_nil, ecx, edx);

  __ mov(ebx, edx);

  // Get new object's map
  qmap.base(ebx);
  __ mov(ebx, qmap);

  // Set proto
  Operand qproto(edx, HObject::kProtoOffset);
  __ mov(qproto, eax);

  // Skip headers
  __ addlb(eax, Immediate(HMap::kSpaceOffset));
  __ addlb(ebx, Immediate(HMap::kSpaceOffset));

  // NOTE: ecx is tagged here

  // Copy all fields from it
  Label loop_start, loop_cond;
  __ jmp(&loop_cond);
  __ bind(&loop_start);

  Operand from(eax, 0), to(ebx, 0);
  __ mov(scratch, from);
  __ mov(to, scratch);

  // Move forward
  __ addlb(eax, Immediate(4));
  __ addlb(ebx, Immediate(4));

  __ dec(ecx);

  // Loop
  __ bind(&loop_cond);
  __ cmpl(ecx, Immediate(0));
  __ jmp(kNe, &loop_start);

  __ mov(eax, edx);

  __ jmp(&done);
  __ bind(&non_object);

  __ mov(ecx, Immediate(HNumber::Tag(16)));

  // Allocate new object
  __ AllocateObjectLiteral(Heap::kTagObject, reg_nil, ecx, eax);

  __ bind(&done);

  GenerateEpilogue();
}


void DeletePropertyStub::Generate() {
  GeneratePrologue();

  // eax <- receiver
  // ebx <- property
  //
  RuntimeDeletePropertyCallback delp = &RuntimeDeleteProperty;

  __ Pushad();

  // RuntimeDeleteProperty(heap, obj, property)
  __ mov(edi, Immediate(reinterpret_cast<intptr_t>(masm()->heap())));
  __ mov(esi, eax);
  __ mov(edx, ebx);
  __ mov(eax, Immediate(*reinterpret_cast<intptr_t*>(&delp)));

  __ push(esi);
  __ push(edx);
  __ push(esi);
  __ push(edi);
  __ call(eax);
  __ addlb(esp, Immediate(4 * 4));

  __ Popad(reg_nil);

  GenerateEpilogue();
}


void HashValueStub::Generate() {
  GeneratePrologue();

  Operand str(ebp, 2 * 4);

  RuntimeGetHashCallback hash = &RuntimeGetHash;

  __ Pushad();

  // RuntimeStringHash(heap, str)
  __ mov(edi, Immediate(reinterpret_cast<intptr_t>(masm()->heap())));
  __ mov(esi, str);
  __ mov(eax, Immediate(*reinterpret_cast<intptr_t*>(&hash)));

  __ push(esi);
  __ push(esi);
  __ push(esi);
  __ push(edi);
  __ call(eax);
  __ addlb(esp, Immediate(4 * 4));

  __ Popad(eax);

  // Caller will unwind stack
  GenerateEpilogue();
}


void StackTraceStub::Generate() {
  // Store caller's frame pointer
  __ mov(ebx, ebp);

  GeneratePrologue();

  // eax <- ip
  // ebx <- ebp
  //
  RuntimeStackTraceCallback strace = &RuntimeStackTrace;

  __ Pushad();

  // RuntimeStackTrace(heap, frame, ip)
  __ mov(edi, Immediate(reinterpret_cast<intptr_t>(masm()->heap())));
  __ mov(esi, ebx);
  __ mov(edx, eax);

  __ push(esi);
  __ push(edx);
  __ push(esi);
  __ push(edi);
  __ mov(eax, Immediate(*reinterpret_cast<intptr_t*>(&strace)));
  __ call(eax);
  __ addlb(esp, Immediate(4 * 4));

  __ Popad(eax);

  GenerateEpilogue();
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
  Heap* heap = masm()->heap();
  Immediate root(reinterpret_cast<intptr_t>(heap->old_space()->root()));
  Operand scratch_op(scratch, 0);

  // eax <- lhs
  // ebx <- rhs

  Label not_unboxed, done;
  Label lhs_to_heap, rhs_to_heap;

  if (type() != BinOp::kDiv) {
    // Try working with unboxed numbers

    __ IsUnboxed(eax, &not_unboxed, NULL);
    __ IsUnboxed(ebx, &not_unboxed, NULL);

    // Number (+) Number
    if (BinOp::is_math(type())) {
      Masm::Spill lvalue(masm(), eax);
      Masm::Spill rvalue(masm(), ebx);

      switch (type()) {
       case BinOp::kAdd: __ addl(eax, ebx); break;
       case BinOp::kSub: __ subl(eax, ebx); break;
       case BinOp::kMul: __ Untag(ebx); __ imull(ebx); break;

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
       case BinOp::kBAnd: __ andl(eax, ebx); break;
       case BinOp::kBOr: __ orl(eax, ebx); break;
       case BinOp::kBXor: __ xorl(eax, ebx); break;
       case BinOp::kMod:
        {
          Label zero;
          __ xorl(edx, edx);
          __ cmpl(ebx, Immediate(HNumber::Tag(0)));
          __ jmp(kEq, &zero);
          __ idivl(ebx);
          __ bind(&zero);
          __ mov(eax, edx);
        }
        break;
       case BinOp::kShl:
       case BinOp::kShr:
       case BinOp::kUShr:
        __ mov(ecx, ebx);
        __ shr(ecx, Immediate(1));

        switch (type()) {
         case BinOp::kShl: __ sal(eax); break;
         case BinOp::kShr: __ sar(eax); break;
         case BinOp::kUShr: __ shr(eax); break;
         default: __ emitb(0xcc); break;
        }

        // Cleanup last bit
        __ shr(eax, Immediate(1));
        __ shl(eax, Immediate(1));

        break;

       default: __ emitb(0xcc); break;
      }
    } else if (BinOp::is_logic(type())) {
      Condition cond = masm()->BinOpToCondition(type(), Masm::kIntegral);
      // Note: eax and ebx are boxed here
      // Otherwise cmp won't work for negative numbers
      __ cmpl(eax, ebx);

      Label true_, cond_end;

      __ mov(scratch, root);
      __ mov(scratch, scratch_op);
      Operand truev(scratch, HContext::GetIndexDisp(Heap::kRootTrueIndex));
      Operand falsev(scratch, HContext::GetIndexDisp(Heap::kRootFalseIndex));

      __ jmp(cond, &true_);

      __ mov(eax, falsev);
      __ jmp(&cond_end);

      __ bind(&true_);

      __ mov(eax, truev);
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

  __ IsNil(eax, NULL, &call_runtime);
  __ IsNil(ebx, NULL, &call_runtime);

  // Convert lhs to heap number if needed
  __ IsUnboxed(eax, &box_rhs, NULL);

  __ Untag(eax);

  __ xorld(xmm1, xmm1);
  __ cvtsi2sd(xmm1, eax);
  __ xorl(eax, eax);
  __ AllocateNumber(xmm1, eax);

  __ bind(&box_rhs);

  // Convert rhs to heap number if needed
  __ IsUnboxed(ebx, &both_boxed, NULL);

  __ Untag(ebx);

  __ xorld(xmm1, xmm1);
  __ cvtsi2sd(xmm1, ebx);
  __ xorl(ebx, ebx);

  __ AllocateNumber(xmm1, ebx);

  // Both lhs and rhs are heap values (not-unboxed)
  __ bind(&both_boxed);

  if (BinOp::is_bool_logic(type())) {
    // Call runtime w/o any checks
    __ jmp(&call_runtime);
  }

  __ IsNil(eax, NULL, &call_runtime);
  __ IsNil(ebx, NULL, &call_runtime);

  __ IsHeapObject(Heap::kTagNumber, eax, &call_runtime, NULL);
  __ IsHeapObject(Heap::kTagNumber, ebx, &call_runtime, NULL);

  // We're adding two heap numbers
  Operand lvalue(eax, HNumber::kValueOffset);
  Operand rvalue(ebx, HNumber::kValueOffset);
  __ movd(xmm1, lvalue);
  __ movd(xmm2, rvalue);
  __ xorl(ebx, ebx);

  if (BinOp::is_math(type())) {
    switch (type()) {
     case BinOp::kAdd: __ addld(xmm1, xmm2); break;
     case BinOp::kSub: __ subld(xmm1, xmm2); break;
     case BinOp::kMul: __ mulld(xmm1, xmm2); break;
     case BinOp::kDiv: __ divld(xmm1, xmm2); break;
     default: __ emitb(0xcc); break;
    }

    __ AllocateNumber(xmm1, eax);
  } else if (BinOp::is_binary(type())) {
    // Truncate lhs and rhs first
    __ cvttsd2si(eax, xmm1);
    __ cvttsd2si(ebx, xmm2);

    switch (type()) {
     case BinOp::kBAnd: __ andl(eax, ebx); break;
     case BinOp::kBOr: __ orl(eax, ebx); break;
     case BinOp::kBXor: __ xorl(eax, ebx); break;
     case BinOp::kMod:
      {
        Label zero;
        __ xorl(edx, edx);
        __ cmpl(ebx, Immediate(HNumber::Tag(0)));
        __ jmp(kEq, &zero);
        __ idivl(ebx);
        __ bind(&zero);
        __ mov(eax, edx);
      }
      break;
     case BinOp::kShl:
     case BinOp::kShr:
     case BinOp::kUShr:
      __ mov(ecx, ebx);

      switch (type()) {
       case BinOp::kUShr:
         __ shl(eax, Immediate(1));
         __ shr(eax);
         __ shr(eax, Immediate(1));
         break;
       case BinOp::kShl: __ shl(eax); break;
       case BinOp::kShr: __ shr(eax); break;
       default: __ emitb(0xcc); break;
      }
      break;
     default: __ emitb(0xcc); break;
    }

    __ TagNumber(eax);
  } else if (BinOp::is_logic(type())) {
    Condition cond = masm()->BinOpToCondition(type(), Masm::kDouble);
    __ mov(scratch, root);
    __ mov(scratch, scratch_op);
    __ ucomisd(xmm1, xmm2);

    Label true_, comp_end;

    Operand truev(scratch, HContext::GetIndexDisp(Heap::kRootTrueIndex));
    Operand falsev(scratch, HContext::GetIndexDisp(Heap::kRootFalseIndex));

    __ jmp(cond, &true_);

    __ mov(eax, falsev);
    __ jmp(&comp_end);

    __ bind(&true_);
    __ mov(eax, truev);
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
  __ mov(edi, heapref);
  __ mov(esi, eax);
  __ mov(edx, ebx);

  __ push(edx);
  __ push(edx);
  __ push(esi);
  __ push(edi);
  __ mov(scratch, Immediate(*reinterpret_cast<intptr_t*>(&cb)));
  __ call(scratch);
  __ addlb(esp, Immediate(4 * 4));

  __ Popad(eax);

  __ bind(&done);

  // Cleanup
  __ xorl(edx, edx);
  __ xorl(ecx, ecx);
  __ xorl(ebx, ebx);

  __ CheckGC();

  GenerateEpilogue();
}

#undef BINARY_SUB_TYPES

void LoadVarArgStub::Generate() {
  __ mov(edx, ebp);
  GeneratePrologue();

  // offset and rest are unboxed
  Register offset = eax;
  Register rest = ebx;
  Register arr = ecx;
  Masm::Spill ebp_s(masm(), edx);
  Operand argc(edx, -HValue::kPointerSize * 2);
  Operand qmap(arr, HObject::kMapOffset);
  Operand slot(scratch, 0);
  Operand stack_slot(offset, 0);

  Label loop, preloop, end;

  // Calculate length of vararg array
  __ mov(scratch, offset);
  __ addl(scratch, rest);

  // If offset + rest <= argc - return immediately
  __ cmpl(scratch, argc);
  __ jmp(kGe, &end);

  // edx = argc - offset - rest
  __ mov(edx, argc);
  __ subl(edx, scratch);

  // Array index
  __ mov(ebx, Immediate(HNumber::Tag(0)));

  Masm::Spill arr_s(masm(), arr), edx_s(masm());
  Masm::Spill offset_s(masm(), offset), ebx_s(masm());

  __ bind(&loop);

  // while (edx > 0)
  __ cmpl(edx, Immediate(HNumber::Tag(0)));
  __ jmp(kEq, &end);

  edx_s.SpillReg(edx);
  ebx_s.SpillReg(ebx);

  __ mov(eax, arr);

  // eax <- object
  // ebx <- property
  __ mov(ecx, Immediate(1));
  __ Call(masm()->stubs()->GetLookupPropertyStub());

  arr_s.Unspill();
  ebx_s.Unspill();

  // Make eax look like unboxed number to GC
  __ dec(eax);
  __ CheckGC();
  __ inc(eax);

  __ IsNil(eax, NULL, &preloop);

  __ mov(arr, qmap);
  __ addl(eax, arr);
  __ mov(scratch, eax);

  // Get stack offset
  offset_s.Unspill();
  __ addlb(offset, Immediate(HNumber::Tag(2)));
  __ addl(offset, ebx);
  __ shl(offset, 1);
  __ addl(offset, *ebp_s.GetOperand());
  __ mov(offset, stack_slot);

  // Put argument in array
  __ mov(slot, offset);

  arr_s.Unspill();

  __ bind(&preloop);

  // Increment array index
  __ addlb(ebx, Immediate(HNumber::Tag(1)));

  // edx --
  edx_s.Unspill();
  __ sublb(edx, Immediate(HNumber::Tag(1)));
  __ jmp(&loop);

  __ bind(&end);

  // Cleanup?
  __ xorl(eax, eax);
  __ xorl(ebx, ebx);
  __ xorl(edx, edx);
  // ecx <- holds result

  GenerateEpilogue();
}


void StoreVarArgStub::Generate() {
  GeneratePrologue();

  Register varg = eax;
  Register index = ebx;
  Register map = ecx;
  Register stack = edx;
  Operand stack_slot(stack, 0);

  // eax <- varg
  Label loop, not_array, odd_end, r1_nil, r2_nil;
  Masm::Spill index_s(masm()), map_s(masm()), array_s(masm()), r1(masm());
  Masm::Spill stack_s(masm(), stack);
  Operand slot(eax, 0);

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
  __ cmpl(index, Immediate(HNumber::Tag(0)));
  __ jmp(kEq, &not_array);

  // index--;
  __ sublb(index, Immediate(HNumber::Tag(1)));

  index_s.SpillReg(index);

  // odd case: array[index]
  __ mov(ebx, index);
  __ mov(ecx, Immediate(0));
  __ Call(masm()->stubs()->GetLookupPropertyStub());

  __ IsNil(eax, NULL, &r1_nil);
  map_s.Unspill();
  __ addl(eax, map);
  __ mov(eax, slot);

  __ bind(&r1_nil);
  r1.SpillReg(eax);

  index_s.Unspill();

  // if (index == 0) goto odd_end;
  __ cmpl(index, Immediate(HNumber::Tag(0)));
  __ jmp(kEq, &odd_end);

  // index--;
  __ sublb(index, Immediate(HNumber::Tag(1)));

  array_s.Unspill();
  index_s.SpillReg(index);

  // even case: array[index]
  __ mov(ebx, index);
  __ mov(ecx, Immediate(0));
  __ Call(masm()->stubs()->GetLookupPropertyStub());

  __ IsNil(eax, NULL, &r2_nil);
  map_s.Unspill();
  __ addl(eax, map);
  __ mov(eax, slot);

  __ bind(&r2_nil);

  // Push two item at the same time (to preserve alignment)
  r1.Unspill(index);
  stack_s.Unspill();
  __ mov(stack_slot, index);
  __ sublb(stack, Immediate(4));
  __ mov(stack_slot, eax);
  __ sublb(stack, Immediate(4));
  stack_s.SpillReg(stack);

  index_s.Unspill();
  array_s.Unspill();

  __ jmp(&loop);

  // }
  __ bind(&odd_end);

  r1.Unspill(eax);
  stack_s.Unspill();
  __ mov(stack_slot, eax);
  __ sublb(stack, Immediate(4));
  stack_s.SpillReg(stack);

  __ bind(&not_array);

  __ xorl(map, map);
  GenerateEpilogue();
}

} // namespace internal
} // namespace candor
