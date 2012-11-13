#include "fullgen.h"
#include "fullgen-inl.h"
#include "fullgen-instructions.h"
#include "fullgen-instructions-inl.h"
#include "heap.h"
#include "heap-inl.h"
#include "macroassembler.h"

namespace candor {
namespace internal {

#define __ masm->

Operand* FOperand::ToOperand() {
  assert(type_ == kStack);

  // Argc and return address
  return new Operand(ebp, -HValue::kPointerSize * (index() + 3));
}


void FNop::Generate(Masm* masm) {
  // Nop
}


void FNil::Generate(Masm* masm) {
  __ mov(eax, Immediate(Heap::kTagNil));
}


void FLabel::Generate(Masm* masm) {
  __ bind(&label);
}


void FEntry::Generate(Masm* masm) {
}


void FReturn::Generate(Masm* masm) {
}


void FChi::Generate(Masm* masm) {
  if (result == NULL) return;

  // Just move input to output
  __ mov(scratch, *inputs[0]->ToOperand());
  __ mov(*result->ToOperand(), scratch);
}


void FFunction::Generate(Masm* masm) {
}


void FLiteral::Generate(Masm* masm) {
}


void FBinOp::Generate(Masm* masm) {
}


void FNot::Generate(Masm* masm) {
}


void FStore::Generate(Masm* masm) {
}


void FLoad::Generate(Masm* masm) {
}


void FStoreContext::Generate(Masm* masm) {
}


void FLoadContext::Generate(Masm* masm) {
}


void FStoreProperty::Generate(Masm* masm) {
}


void FLoadProperty::Generate(Masm* masm) {
}


void FDeleteProperty::Generate(Masm* masm) {
}


void FAllocateObject::Generate(Masm* masm) {
}


void FAllocateArray::Generate(Masm* masm) {
}


void FClone::Generate(Masm* masm) {
}


void FSizeof::Generate(Masm* masm) {
}


void FKeysof::Generate(Masm* masm) {
}


void FTypeof::Generate(Masm* masm) {
}


void FBreak::Generate(Masm* masm) {
}


void FContinue::Generate(Masm* masm) {
}


void FIf::Generate(Masm* masm) {
}


void FGoto::Generate(Masm* masm) {
}


void FStoreArg::Generate(Masm* masm) {
}


void FStoreVarArg::Generate(Masm* masm) {
}


void FLoadArg::Generate(Masm* masm) {
}


void FLoadVarArg::Generate(Masm* masm) {
}


void FAlignStack::Generate(Masm* masm) {
}


void FCollectGarbage::Generate(Masm* masm) {
}


void FGetStackTrace::Generate(Masm* masm) {
}


void FCall::Generate(Masm* masm) {
}

} // namespace internal
} // namespace candor
