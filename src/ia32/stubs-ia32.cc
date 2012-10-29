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
  __ push(ebp);
  __ movl(ebp, esp);
}


void BaseStub::GenerateEpilogue(int args) {
  __ movl(esp, ebp);
  __ pop(ebp);

  // tag + size
  __ ret(args * 4);
}


void EntryStub::Generate() {
  GeneratePrologue();

  // Align stack and allocate some spill slots
  // (for root_op)
  __ subl(esp, Immediate(3 * 4));

  Operand fn(ebp, 2 * 4);
  Operand argc(ebp, 3 * 4);
  Operand argv(ebp, 4 * 4);

  // Store registers
  __ push(ebx);
  __ push(esi);
  __ push(edi);

  __ movl(edi, fn);
  __ movl(esi, argc);
  __ movl(edx, argv);

  // edi <- function addr
  // esi <- unboxed arguments count (tagged)
  // edx <- pointer to arguments array

  __ EnterFramePrologue();

  // Push all arguments to stack
  Label even, args, args_loop, unwind_even;
  __ movl(eax, esi);
  __ Untag(eax);

  // Odd arguments count check (for alignment)
  __ testb(eax, Immediate(1));
  __ jmp(kEq, &even);
  __ push(Immediate(0));
  __ bind(&even);

  // Get pointer to the end of arguments array
  __ movl(ebx, eax);
  __ shl(ebx, Immediate(2));
  __ addl(ebx, edx);

  __ jmp(&args_loop);

  __ bind(&args);

  __ subl(ebx, Immediate(4));

  // Get argument from list
  Operand arg(ebx, 0);
  __ movl(eax, arg);
  __ push(eax);

  // Loop if needed
  __ bind(&args_loop);
  __ cmpl(ebx, edx);
  __ jmp(kNe, &args);

  // Nullify all registers to help GC distinguish on-stack values
  __ xorl(eax, eax);
  __ xorl(ebx, ebx);
  __ xorl(ecx, ecx);
  __ xorl(edx, edx);

  // Call code
  __ movl(scratch, edi);
  __ CallFunction(scratch);

  // Unwind arguments
  __ movl(esi, argc);
  __ Untag(esi);

  // XXX: testb(esi, ...) transforms to testb(edx, ...) WAT??!
  __ testl(esi, Immediate(1));
  __ jmp(kEq, &unwind_even);
  __ inc(esi);
  __ bind(&unwind_even);

  __ shl(esi, Immediate(2));
  __ addl(esp, esi);

  __ EnterFrameEpilogue();

  // Restore registers
  __ pop(edi);
  __ pop(esi);
  __ pop(ebx);

  GenerateEpilogue(0);
}


void AllocateStub::Generate() {
  GeneratePrologue();
  // Align stack
  __ push(Immediate(0));
  __ push(edx);

  // Arguments
  Operand size(ebp, 3 * 4);
  Operand tag(ebp, 2 * 4);

  Label runtime_allocate, done;

  Heap* heap = masm()->heap();
  Immediate heapref(reinterpret_cast<uint32_t>(heap));
  Immediate top(reinterpret_cast<uint32_t>(heap->new_space()->top()));
  Immediate limit(reinterpret_cast<uint32_t>(heap->new_space()->limit()));

  Operand scratch_op(scratch, 0);

  // Get pointer to current page's top
  // (new_space()->top() is a pointer to space's property
  // which is a pointer to page's top pointer
  // that's why we are dereferencing it here twice
  __ movl(scratch, top);
  __ movl(scratch, scratch_op);
  __ movl(eax, scratch_op);
  __ movl(edx, size);
  __ Untag(edx);

  // Add object size to the top
  __ addl(edx, eax);
  __ jmp(kCarry, &runtime_allocate);

  // Check if we exhausted buffer
  __ movl(scratch, limit);
  __ movl(scratch, scratch_op);
  __ cmpl(edx, scratch_op);
  __ jmp(kGt, &runtime_allocate);

  // We should allocate only even amount of bytes
  __ orlb(edx, Immediate(0x01));

  // Update top
  __ movl(scratch, top);
  __ movl(scratch, scratch_op);
  __ movl(scratch_op, edx);

  __ jmp(&done);

  // Invoke runtime allocation stub
  __ bind(&runtime_allocate);

  // Remove junk from registers
  __ xorl(eax, eax);
  __ xorl(edx, edx);

  RuntimeAllocateCallback allocate = &RuntimeAllocate;

  {
    __ ChangeAlign(2);
    Masm::Align a(masm());
    __ Pushad();

    // Two arguments: heap, size
    __ movl(scratch, size);
    __ push(scratch);
    __ push(heapref);

    __ movl(scratch, Immediate(*reinterpret_cast<uint32_t*>(&allocate)));

    __ Call(scratch);
    __ addl(esp, Immediate(2 * 4));

    __ Popad(eax);
    __ ChangeAlign(-2);
  }

  // Voila result and result_end are pointers
  __ bind(&done);

  // Set tag
  Operand qtag(eax, HValue::kTagOffset);
  __ movl(scratch, tag);
  __ Untag(scratch);
  __ movl(qtag, scratch);

  // eax will hold resulting pointer
  __ pop(edx);
  GenerateEpilogue(2);
}


void CallBindingStub::Generate() {
  GeneratePrologue();

  Operand argc(ebp, 3 * 4);
  Operand fn(ebp, 2 * 4);

  // Save all registers
  __ Pushad();

  // binding(argc, argv)
  __ movl(edi, argc);
  __ Untag(edi);
  __ movl(esi, ebp);

  // old ebp + return address + two arguments
  __ addl(esi, Immediate(4 * 4));
  __ movl(scratch, edi);
  __ shl(scratch, Immediate(2));
  __ subl(esi, scratch);

  // argv should point to the end of arguments array
  __ movl(scratch, edi);
  __ shl(scratch, Immediate(2));
  __ addl(esi, scratch);

  __ ExitFramePrologue();

  Operand code(scratch, HFunction::kCodeOffset);

  __ movl(scratch, fn);
  __ Call(code);

  __ ExitFrameEpilogue();

  // Restore all except eax
  __ Popad(eax);

  __ CheckGC();
  GenerateEpilogue(2);
}


void VarArgStub::Generate() {
  GeneratePrologue();

  __ AllocateSpills(0);

  // eax <- interior pointer to arguments
  // edx <- arguments count (to put into array)
  __ push(esi);
  __ push(ecx);

  Masm::Spill argv_s(masm(), eax);
  Masm::Spill argc_s(masm(), edx);

  // Allocate array
  __ movl(ecx, Immediate(HNumber::Tag(PowerOfTwo(HArray::kVarArgLength))));
  __ AllocateObjectLiteral(Heap::kTagArray, ecx, eax);

  // Array index
  __ xorl(esi, esi);

  Label loop_start, loop_cond;

  argc_s.Unspill(ecx);
  __ jmp(&loop_cond);
  __ bind(&loop_start);

  // Insert entry : eax[esi] = *eax_s
  __ push(eax);
  __ push(esi);

  // Key insertion flag
  __ movl(ecx, Immediate(1));
  __ Call(masm()->stubs()->GetLookupPropertyStub());

  // Calculate pointer
  __ movl(edx, eax);
  __ pop(esi);
  __ pop(eax);

  Operand qmap(eax, HObject::kMapOffset);
  __ addl(edx, qmap);

  // Put value into the slot
  Operand slot(edx, 0);
  Operand value(eax, 0);

  argv_s.Unspill(eax);
  __ movl(eax, value);
  __ movl(slot, scratch);

  // Move forward
  __ addl(eax, Immediate(4));
  argv_s.SpillReg(eax);

  argc_s.Unspill(ecx);
  __ subl(scratch, Immediate(4));
  argc_s.SpillReg(ecx);

  __ addl(esi, Immediate(HNumber::Tag(1)));

  __ bind(&loop_cond);

  // r14 != 0
  __ cmpl(ecx, Immediate(0));
  __ jmp(kNe, &loop_start);

  __ pop(ecx);
  __ pop(esi);

  // Cleanup
  __ xorl(eax, eax);
  __ xorl(esi, esi);

  __ CheckGC();

  __ FinalizeSpills();

  GenerateEpilogue(0);
}


void PutVarArgStub::Generate() {
  GeneratePrologue();

  __ AllocateSpills(0);

  // eax <- array
  // ebx <- stack offset
  Masm::Spill eax_s(masm(), eax), stack_s(masm(), ebx);

  Operand qlength(eax, HArray::kLengthOffset);
  __ movl(scratch, qlength);
  __ TagNumber(scratch);

  Masm::Spill scratch_s(masm(), scratch);

  Label loop_start, loop_cond;

  // ebx <- index
  __ movl(ebx, Immediate(HNumber::Tag(0)));

  __ jmp(&loop_cond);
  __ bind(&loop_start);

  {
    Masm::Spill ebx_s(masm(), ebx);

    eax_s.Unspill();
    __ movl(ecx, Immediate(0));

    // eax <- array
    // ebx <- index
    // ecx <- flag(0)
    __ Call(masm()->stubs()->GetLookupPropertyStub());
    eax_s.Unspill(scratch);
    Operand qmap(scratch, HObject::kMapOffset);
    Operand qself(eax, 0);
    __ addl(eax, qmap);
    __ movl(eax, qself);

    stack_s.Unspill(edx);
    Operand slot(edx, 0);
    __ movl(slot, eax);
    __ addl(edx, Immediate(4));
    stack_s.SpillReg(edx);

    ebx_s.Unspill();
  }

  __ addl(ebx, Immediate(HNumber::Tag(1)));

  scratch_s.Unspill();
  __ bind(&loop_cond);

  // while (ebx < scratch)
  __ cmpl(ebx, scratch);
  __ jmp(kLt, &loop_start);

  __ FinalizeSpills();

  GenerateEpilogue(0);
}


void CollectGarbageStub::Generate() {
  GeneratePrologue();

  RuntimeCollectGarbageCallback gc = &RuntimeCollectGarbage;
  __ Pushad();

  {
    __ ChangeAlign(2);
    Masm::Align a(masm());

    // RuntimeCollectGarbage(heap, stack_top)
    __ push(esp);
    __ push(Immediate(reinterpret_cast<uint32_t>(masm()->heap())));
    __ movl(eax, Immediate(*reinterpret_cast<uint32_t*>(&gc)));
    __ Call(eax);
    __ addl(esp, Immediate(2 * 4));

    __ ChangeAlign(-2);
  }

  __ Popad(reg_nil);

  GenerateEpilogue(0);
}


void TypeofStub::Generate() {
  GeneratePrologue();

  // edx <- root register

  Label not_nil, not_unboxed, done;

  // Typeof 1 = 'number'
  __ IsUnboxed(eax, &not_unboxed, NULL);
  __ movl(eax, Immediate(HContext::GetIndexDisp(Heap::kRootNumberTypeIndex)));

  __ jmp(&done);
  __ bind(&not_unboxed);

  // Typeof nil = 'nil'
  __ IsNil(eax, &not_nil, NULL);

  __ movl(eax, Immediate(HContext::GetIndexDisp(Heap::kRootNilTypeIndex)));
  __ jmp(&done);
  __ bind(&not_nil);

  Operand btag(eax, HValue::kTagOffset);
  __ movzxb(eax, btag);
  __ shl(eax, Immediate(2));
  __ addl(eax, Immediate(HContext::GetIndexDisp(
          Heap::kRootBooleanTypeIndex - Heap::kTagBoolean)));

  __ bind(&done);

  // eax contains offset in root_reg
  Operand type(eax, 0);
  __ addl(eax, edx);
  __ movl(eax, type);

  GenerateEpilogue(0);
}


void SizeofStub::Generate() {
  GeneratePrologue();
  RuntimeSizeofCallback sizeofc = &RuntimeSizeof;

  __ Pushad();

  // RuntimeSizeof(heap, obj)
  {
    __ ChangeAlign(2);
    Masm::Align a(masm());
    __ push(eax);
    __ push(Immediate(reinterpret_cast<uint32_t>(masm()->heap())));
    __ movl(eax, Immediate(*reinterpret_cast<uint32_t*>(&sizeofc)));
    __ call(eax);

    // Unwind stack
    __ addl(esp, Immediate(2 * 4));
    __ ChangeAlign(-2);
  }

  __ Popad(eax);

  GenerateEpilogue(0);
}


void KeysofStub::Generate() {
  GeneratePrologue();
  RuntimeKeysofCallback keysofc = &RuntimeKeysof;

  __ Pushad();

  // RuntimeKeysof(heap, obj)
  {
    __ ChangeAlign(2);
    Masm::Align a(masm());

    __ push(eax);
    __ push(Immediate(reinterpret_cast<uint32_t>(masm()->heap())));
    __ movl(eax, Immediate(*reinterpret_cast<uint32_t*>(&keysofc)));
    __ call(eax);
    __ addl(esp, Immediate(2 * 4));

    __ ChangeAlign(-2);
  }

  __ Popad(eax);

  GenerateEpilogue(0);
}


void LookupPropertyStub::Generate() {
  GeneratePrologue();
  __ AllocateSpills(0);

  // Save registers and align
  __ push(esi);
  __ push(edi);
  __ push(ebx);
  __ push(ecx);

  Label is_object, is_array, cleanup, slow_case;
  Label non_object_error, done;

  // eax <- object
  // edx <- property
  // ecx <- change flag

  Masm::Spill object_s(masm(), eax);
  Masm::Spill key_s(masm(), edx);
  Masm::Spill change_s(masm(), ecx);

  // Return nil on non-object's property access
  __ IsUnboxed(eax, NULL, &non_object_error);
  __ IsNil(eax, NULL, &non_object_error);

  // Or into non-object
  __ IsHeapObject(Heap::kTagObject, eax, NULL, &is_object);
  __ IsHeapObject(Heap::kTagArray, eax, &non_object_error, &is_array);

  __ bind(&is_object);

  // Fast case: object and a string key
  {
    __ IsUnboxed(edx, NULL, &slow_case);
    __ IsNil(edx, NULL, &slow_case);
    __ IsHeapObject(Heap::kTagString, edx, &slow_case, NULL);

    __ StringHash(edx, ebx);

    Operand qmask(eax, HObject::kMaskOffset);
    __ movl(eax, qmask);

    // offset = hash & mask + kSpaceOffset
    __ andl(ebx, eax);
    __ addl(ebx, Immediate(HMap::kSpaceOffset));

    object_s.Unspill(eax);

    Operand qmap(eax, HObject::kMapOffset);
    __ movl(esi, qmap);
    __ addl(esi, ebx);

    Label match;

    // ebx now contains pointer to the key slot in map's space
    // compare key's addresses
    Operand slot(esi, 0);
    __ movl(esi, slot);

    // Slot should contain either key
    __ cmpl(esi, edx);
    __ jmp(kEq, &match);

    // or nil
    __ cmpl(esi, Immediate(Heap::kTagNil));
    __ jmp(kNe, &cleanup);

    __ bind(&match);

    Label fast_case_end;

    // Insert key if was asked
    __ cmpl(ecx, Immediate(0));
    __ jmp(kEq, &fast_case_end);

    // Restore map's interior pointer
    __ movl(esi, qmap);
    __ addl(esi, ebx);

    // Put the key into slot
    __ movl(slot, edx);

    __ bind(&fast_case_end);

    // Compute value's address
    // eax = key_offset + mask + 4
    object_s.Unspill(eax);
    __ movl(eax, qmask);
    __ addl(eax, ebx);
    __ addl(eax, Immediate(HValue::kPointerSize));

    // Cleanup
    __ xorl(ebx, ebx);

    // Return value
    GenerateEpilogue(0);
  }

  __ bind(&is_array);
  // Fast case: dense array and a unboxed key
  {
    __ IsUnboxed(edx, &slow_case, NULL);
    __ IsNil(edx, NULL, &slow_case);
    __ cmpl(edx, Immediate(-1));
    __ jmp(kLe, &slow_case);
    __ IsDenseArray(eax, &slow_case, NULL);

    // Get mask
    Operand qmask(eax, HObject::kMaskOffset);
    __ movl(ebx, qmask);

    // Check if index is above the mask
    // NOTE: edx is tagged so we need to shift it only 2 times
    __ shl(edx, Immediate(2));
    __ cmpl(edx, ebx);
    __ jmp(kGt, &cleanup);

    // Apply mask
    __ andl(edx, ebx);
    Masm::Spill mask_s(masm(), edx);
    key_s.Unspill(edx);

    // Check if length was increased
    Label length_set;

    Operand qlength(eax, HArray::kLengthOffset);
    __ movl(ebx, qlength);
    __ Untag(edx);
    __ inc(edx);
    __ cmpl(edx, ebx);
    __ jmp(kLe, &length_set);

    // Update length
    __ movl(qlength, edx);

    __ bind(&length_set);
    // edx is untagged here - so nullify it
    __ xorl(edx, edx);

    // Get index
    mask_s.Unspill(eax);
    __ addl(eax, Immediate(HMap::kSpaceOffset));

    // Cleanup
    __ xorl(ebx, ebx);

    // Return value
    GenerateEpilogue(0);
  }

  __ bind(&cleanup);

  __ xorl(ebx, ebx);

  object_s.Unspill();
  key_s.Unspill();

  __ bind(&slow_case);

  __ Pushad();

  RuntimeLookupPropertyCallback lookup = &RuntimeLookupProperty;

  {
    __ ChangeAlign(4);
    Masm::Align a(masm());

    // RuntimeLookupProperty(heap, obj, key, change)
    // (returns addr of slot)
    __ push(ecx);
    __ push(edx);
    __ push(eax);
    __ push(Immediate(reinterpret_cast<uint32_t>(masm()->heap())));
    // ecx already contains change flag
    __ movl(eax, Immediate(*reinterpret_cast<uint32_t*>(&lookup)));
    __ call(eax);
    __ addl(esp, Immediate(4 * 4));

    __ ChangeAlign(-4);
  }

  __ Popad(eax);

  __ jmp(&done);

  __ bind(&non_object_error);

  // Non object lookups return nil
  __ movl(eax, Immediate(Heap::kTagNil));

  __ bind(&done);

  __ pop(ecx);
  __ pop(edx);
  __ pop(edi);
  __ pop(esi);
  __ FinalizeSpills();
  GenerateEpilogue(0);
}


void CoerceToBooleanStub::Generate() {
  GeneratePrologue();

  // edx <- root register

  Label unboxed, truel, not_bool, coerced_type;

  // Check type and coerce if not boolean
  __ IsUnboxed(eax, NULL, &unboxed);
  __ IsNil(eax, NULL, &not_bool);
  __ IsHeapObject(Heap::kTagBoolean, eax, &not_bool, NULL);

  __ jmp(&coerced_type);

  __ bind(&unboxed);

  Operand truev(edx, HContext::GetIndexDisp(Heap::kRootTrueIndex));
  Operand falsev(edx, HContext::GetIndexDisp(Heap::kRootFalseIndex));

  __ cmpl(eax, Immediate(HNumber::Tag(0)));
  __ jmp(kNe, &truel);

  __ movl(eax, falsev);

  __ jmp(&coerced_type);
  __ bind(&truel);

  __ movl(eax, truev);

  __ jmp(&coerced_type);
  __ bind(&not_bool);

  __ Pushad();

  RuntimeCoerceCallback to_boolean = &RuntimeToBoolean;

  {
    __ ChangeAlign(2);
    Masm::Align a(masm());

    __ push(eax);
    __ push(Immediate(reinterpret_cast<uint32_t>(masm()->heap())));

    __ movl(eax, Immediate(*reinterpret_cast<uint32_t*>(&to_boolean)));
    __ call(eax);

    __ ChangeAlign(-2);
  }

  __ Popad(eax);

  __ bind(&coerced_type);

  __ CheckGC();

  GenerateEpilogue(0);
}


void CloneObjectStub::Generate() {
  GeneratePrologue();
  __ AllocateSpills(0);

  // Align and save
  __ push(esi);
  __ push(edi);
  __ push(ebx);
  __ push(ecx);

  Label non_object, done;

  // eax <- object
  __ IsUnboxed(eax, NULL, &non_object);
  __ IsNil(eax, NULL, &non_object);
  __ IsHeapObject(Heap::kTagObject, eax, &non_object, NULL);

  // Get map
  Operand qmap(eax, HObject::kMapOffset);
  __ movl(eax, qmap);

  // Get size
  Operand qsize(eax, HMap::kSizeOffset);
  __ movl(ecx, qsize);

  __ TagNumber(ecx);

  // Allocate new object
  __ AllocateObjectLiteral(Heap::kTagObject, ecx, edx);

  __ movl(ebx, edx);

  // Get new object's map
  qmap.base(ebx);
  __ movl(ebx, qmap);

  // Skip headers
  __ addl(eax, Immediate(HMap::kSpaceOffset));
  __ addl(ebx, Immediate(HMap::kSpaceOffset));

  // NOTE: ecx is tagged here

  // Copy all fields from it
  Label loop_start, loop_cond;
  __ jmp(&loop_cond);
  __ bind(&loop_start);

  Operand from(eax, 0), to(ebx, 0);
  __ movl(esi, from);
  __ movl(to, esi);

  // Move forward
  __ addl(eax, Immediate(4));
  __ addl(ebx, Immediate(4));

  __ dec(ecx);

  // Loop
  __ bind(&loop_cond);
  __ cmpl(ecx, Immediate(0));
  __ jmp(kNe, &loop_start);

  __ movl(eax, edx);

  __ jmp(&done);
  __ bind(&non_object);

  // Non-object cloning - nil result
  __ movl(eax, Immediate(Heap::kTagNil));

  __ bind(&done);

  __ FinalizeSpills();

  __ pop(ecx);
  __ pop(ebx);
  __ pop(esi);
  __ pop(edi);
  GenerateEpilogue(0);
}


void DeletePropertyStub::Generate() {
  GeneratePrologue();

  // eax <- receiver
  // ebx <- property
  //
  RuntimeDeletePropertyCallback delp = &RuntimeDeleteProperty;

  __ Pushad();

  // RuntimeDeleteProperty(heap, obj, property)
  __ movl(edi, Immediate(reinterpret_cast<uint32_t>(masm()->heap())));
  __ movl(esi, eax);
  __ movl(edx, ebx);
  __ movl(eax, Immediate(*reinterpret_cast<uint32_t*>(&delp)));
  __ call(eax);

  __ Popad(reg_nil);

  // Delete property returns nil
  __ movl(eax, Immediate(Heap::kTagNil));

  GenerateEpilogue(0);
}


void HashValueStub::Generate() {
  GeneratePrologue();

  Operand str(ebp, 2 * 4);

  RuntimeGetHashCallback hash = &RuntimeGetHash;

  __ Pushad();

  // RuntimeStringHash(heap, str)
  __ movl(edi, Immediate(reinterpret_cast<uint32_t>(masm()->heap())));
  __ movl(esi, str);
  __ movl(eax, Immediate(*reinterpret_cast<uint32_t*>(&hash)));
  __ call(eax);

  __ Popad(eax);

  // Caller will unwind stack
  GenerateEpilogue(0);
}


void StackTraceStub::Generate() {
  // Store caller's frame pointer
  __ movl(ebx, ebp);

  GeneratePrologue();

  // eax <- ip
  // ebx <- ebp
  //
  RuntimeStackTraceCallback strace = &RuntimeStackTrace;

  __ Pushad();

  // RuntimeStackTrace(heap, frame, ip)
  __ movl(edi, Immediate(reinterpret_cast<uint32_t>(masm()->heap())));
  __ movl(esi, ebx);
  __ movl(edx, eax);

  __ movl(eax, Immediate(*reinterpret_cast<uint32_t*>(&strace)));
  __ call(eax);

  __ Popad(eax);

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

  // eax <- lhs
  // ebx <- rhs
  // edx <- root register

  // Allocate space for spill slots
  __ AllocateSpills(0);

  Label not_unboxed, done;
  Label lhs_to_heap, rhs_to_heap;

  if (type() != BinOp::kDiv) {
    // Try working with unboxed numbers

    __ IsUnboxed(eax, &not_unboxed, NULL);
    __ IsUnboxed(ecx, &not_unboxed, NULL);

    // Number (+) Number
    if (BinOp::is_math(type())) {
      Masm::Spill lvalue(masm(), eax);
      Masm::Spill rvalue(masm(), ecx);

      switch (type()) {
       case BinOp::kAdd: __ addl(eax, ecx); break;
       case BinOp::kSub: __ subl(eax, ecx); break;
       case BinOp::kMul: __ Untag(ecx); __ imull(ecx); break;

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
       case BinOp::kBAnd: __ andl(eax, ecx); break;
       case BinOp::kBOr: __ orl(eax, ecx); break;
       case BinOp::kBXor: __ xorl(eax, ecx); break;
       case BinOp::kMod:
        __ xorl(edx, edx);
        __ idivl(ecx);
        __ movl(eax, edx);
        break;
       case BinOp::kShl:
       case BinOp::kShr:
       case BinOp::kUShr:
        __ movl(ebx, ecx);
        __ shr(ebx, Immediate(1));

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
      // Note: eax and ecx are boxed here
      // Otherwise cmp won't work for negative numbers
      __ cmpl(eax, ecx);

      Label true_, cond_end;

      Operand truev(edx, HContext::GetIndexDisp(Heap::kRootTrueIndex));
      Operand falsev(edx, HContext::GetIndexDisp(Heap::kRootFalseIndex));

      __ jmp(cond, &true_);

      __ movl(eax, falsev);
      __ jmp(&cond_end);

      __ bind(&true_);

      __ movl(eax, truev);
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
  __ IsNil(ecx, NULL, &call_runtime);

  // Convert lhs to heap number if needed
  __ IsUnboxed(eax, &box_rhs, NULL);

  __ Untag(eax);

  __ xorld(xmm1, xmm1);
  __ cvtsi2sd(xmm1, eax);
  __ xorl(eax, eax);
  __ AllocateNumber(xmm1, eax);

  __ bind(&box_rhs);

  // Convert rhs to heap number if needed
  __ IsUnboxed(ecx, &both_boxed, NULL);

  __ Untag(ecx);

  __ xorld(xmm1, xmm1);
  __ cvtsi2sd(xmm1, ecx);
  __ xorl(ecx, ecx);

  __ AllocateNumber(xmm1, ecx);

  // Both lhs and rhs are heap values (not-unboxed)
  __ bind(&both_boxed);

  if (BinOp::is_bool_logic(type())) {
    // Call runtime w/o any checks
    __ jmp(&call_runtime);
  }

  __ IsNil(eax, NULL, &call_runtime);
  __ IsNil(ecx, NULL, &call_runtime);

  __ IsHeapObject(Heap::kTagNumber, eax, &call_runtime, NULL);
  __ IsHeapObject(Heap::kTagNumber, ecx, &call_runtime, NULL);

  // We're adding two heap numbers
  Operand lvalue(eax, HNumber::kValueOffset);
  Operand rvalue(ecx, HNumber::kValueOffset);
  __ movdqu(lvalue, xmm1);
  __ movdqu(rvalue, xmm2);
  __ xorl(ecx, ecx);

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
    __ cvttsd2si(ecx, xmm2);

    switch (type()) {
     case BinOp::kBAnd: __ andl(eax, ecx); break;
     case BinOp::kBOr: __ orl(eax, ecx); break;
     case BinOp::kBXor: __ xorl(eax, ecx); break;
     case BinOp::kMod:
      __ xorl(edx, edx);
      __ idivl(ecx);
      __ movl(eax, edx);
      break;
     case BinOp::kShl:
     case BinOp::kShr:
     case BinOp::kUShr:
      __ movl(ebx, ecx);

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
    __ ucomisd(xmm1, xmm2);

    Label true_, comp_end;

    Operand truev(edx, HContext::GetIndexDisp(Heap::kRootTrueIndex));
    Operand falsev(edx, HContext::GetIndexDisp(Heap::kRootFalseIndex));

    __ jmp(cond, &true_);

    __ movl(eax, falsev);
    __ jmp(&comp_end);

    __ bind(&true_);
    __ movl(eax, truev);
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

  Immediate heapref(reinterpret_cast<uint32_t>(masm()->heap()));

  // binop(heap, lhs, rhs)
  {
    __ ChangeAlign(3);
    Masm::Align a(masm());

    __ push(ecx);
    __ push(eax);
    __ push(heapref);

    __ movl(scratch, Immediate(*reinterpret_cast<uint32_t*>(&cb)));
    __ call(scratch);
    __ addl(esp, Immediate(4 * 3));

    __ ChangeAlign(-3);
  }

  __ Popad(eax);

  __ bind(&done);

  // Cleanup
  __ xorl(ebx, ebx);
  __ xorl(ecx, ecx);

  __ CheckGC();

  __ FinalizeSpills();

  GenerateEpilogue(0);
}

#undef BINARY_SUB_TYPES

} // namespace internal
} // namespace candor
