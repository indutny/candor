#include "lir.h"
#include "lir-inl.h"
#include "lir-instructions.h"
#include "lir-instructions-inl.h"
#include "macroassembler.h"

namespace candor {
namespace internal {

void LNop::Generate(Masm* masm) {
}


void LNil::Generate(Masm* masm) {
}


void LMove::Generate(Masm* masm) {
}


void LEntry::Generate(Masm* masm) {
}


void LReturn::Generate(Masm* masm) {
}


void LFunction::Generate(Masm* masm) {
}


void LLoadContext::Generate(Masm* masm) {
}


void LStoreContext::Generate(Masm* masm) {
}


void LLoadProperty::Generate(Masm* masm) {
}


void LStoreProperty::Generate(Masm* masm) {
}


void LDeleteProperty::Generate(Masm* masm) {
}


void LLiteral::Generate(Masm* masm) {
}


void LNot::Generate(Masm* masm) {
}


void LBinOp::Generate(Masm* masm) {
}


void LTypeof::Generate(Masm* masm) {
}


void LSizeof::Generate(Masm* masm) {
}


void LKeysof::Generate(Masm* masm) {
}


void LClone::Generate(Masm* masm) {
}


void LCall::Generate(Masm* masm) {
}


void LCollectGarbage::Generate(Masm* masm) {
}


void LGetStackTrace::Generate(Masm* masm) {
}


void LAllocateObject::Generate(Masm* masm) {
}


void LAllocateArray::Generate(Masm* masm) {
}


void LPhi::Generate(Masm* masm) {
}


void LLabel::Generate(Masm* masm) {
}


void LGap::Generate(Masm* masm) {
}


void LLoadArg::Generate(Masm* masm) {
}


void LBranch::Generate(Masm* masm) {
}


void LGoto::Generate(Masm* masm) {
}

} // namespace internal
} // namespace candor
