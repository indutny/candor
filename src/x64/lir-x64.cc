#include "lir.h"
#include "lir-inl.h"
#include "lir-instructions.h"
#include "lir-instructions-inl.h"
#include "macroassembler.h"

namespace candor {
namespace internal {

void LGen::VisitNop(HIRInstruction* instr) {
}


void LGen::VisitNil(HIRInstruction* instr) {
  Bind(LInstruction::kNil)
      ->SetResult(CreateVirtual(), LUse::kAny);
}


void LGen::VisitEntry(HIRInstruction* instr) {
  Bind(LInstruction::kEntry);
}


void LGen::VisitReturn(HIRInstruction* instr) {
  Bind(LInstruction::kReturn)
      ->AddArg(ToFixed(instr->left(), rax), LUse::kRegister);
}


void LGen::VisitLiteral(HIRInstruction* instr) {
  Bind(LInstruction::kLiteral)
      ->SetResult(CreateVirtual(), LUse::kRegister);
}


void LGen::VisitAllocateObject(HIRInstruction* instr) {
  Bind(LInstruction::kAllocateObject)
      ->SetResult(CreateVirtual(), LUse::kRegister);
}


void LGen::VisitAllocateArray(HIRInstruction* instr) {
  Bind(LInstruction::kAllocateArray)
      ->SetResult(CreateVirtual(), LUse::kRegister);
}


void LGen::VisitFunction(HIRInstruction* instr) {
  // XXX : Store address somewhere
  Bind(LInstruction::kFunction)
      ->SetResult(CreateVirtual(), LUse::kRegister);
}


void LGen::VisitNot(HIRInstruction* instr) {
  Bind(LInstruction::kNot)
      ->AddArg(ToFixed(instr->left(), rax), LUse::kRegister)
      ->SetResult(FromFixed(rax, CreateVirtual()), LUse::kRegister);
}


void LGen::VisitBinOp(HIRInstruction* instr) {
  Bind(LInstruction::kBinOp)
      ->AddArg(ToFixed(instr->left(), rax), LUse::kRegister)
      ->AddArg(ToFixed(instr->right(), rbx), LUse::kRegister)
      ->SetResult(FromFixed(rax, CreateVirtual()), LUse::kRegister);
}


void LGen::VisitSizeof(HIRInstruction* instr) {
  Bind(LInstruction::kSizeof)
      ->AddArg(ToFixed(instr->left(), rax), LUse::kRegister)
      ->SetResult(CreateVirtual(), LUse::kRegister);
}


void LGen::VisitTypeof(HIRInstruction* instr) {
  Bind(LInstruction::kTypeof)
      ->AddArg(ToFixed(instr->left(), rax), LUse::kRegister)
      ->SetResult(CreateVirtual(), LUse::kRegister);
}


void LGen::VisitKeysof(HIRInstruction* instr) {
  Bind(LInstruction::kKeysof)
      ->AddArg(ToFixed(instr->left(), rax), LUse::kRegister)
      ->SetResult(CreateVirtual(), LUse::kRegister);
}


void LGen::VisitClone(HIRInstruction* instr) {
  Bind(LInstruction::kClone)
      ->AddArg(ToFixed(instr->left(), rax), LUse::kRegister)
      ->SetResult(CreateVirtual(), LUse::kRegister);
}


void LGen::VisitLoadContext(HIRInstruction* instr) {
  Bind(LInstruction::kLoadContext)
      ->SetSlot(instr->slot())
      ->SetResult(CreateVirtual(), LUse::kRegister);
}


void LGen::VisitStoreContext(HIRInstruction* instr) {
  Bind(LInstruction::kStoreContext)
      ->SetSlot(instr->slot())
      ->AddArg(instr->left(), LUse::kRegister)
      ->SetResult(CreateVirtual(), LUse::kRegister);
}


void LGen::VisitLoadProperty(HIRInstruction* instr) {
  Bind(LInstruction::kLoadProperty)
      ->AddScratch(CreateVirtual())
      ->AddArg(ToFixed(instr->left(), rax), LUse::kRegister)
      ->AddArg(ToFixed(instr->right(), rbx), LUse::kRegister)
      ->SetResult(CreateVirtual(), LUse::kRegister);
}


void LGen::VisitStoreProperty(HIRInstruction* instr) {
  LInstruction* load = Bind(LInstruction::kStoreProperty)
      ->MarkHasCall()
      ->AddArg(ToFixed(instr->left(), rax), LUse::kRegister)
      ->AddArg(ToFixed(instr->right(), rbx), LUse::kRegister)
      ->SetResult(ToFixed(instr->third(), rcx), LUse::kRegister);
}


void LGen::VisitDeleteProperty(HIRInstruction* instr) {
  Bind(LInstruction::kDeleteProperty)
      ->AddArg(ToFixed(instr->left(), rax), LUse::kRegister)
      ->AddArg(ToFixed(instr->right(), rbx), LUse::kRegister);
}


void LGen::VisitGetStackTrace(HIRInstruction* instr) {
  Bind(LInstruction::kGetStackTrace);
}


void LGen::VisitCollectGarbage(HIRInstruction* instr) {
  Bind(LInstruction::kCollectGarbage);
}


void LGen::VisitLoadArg(HIRInstruction* instr) {
  Bind(LInstruction::kLoadArg)->SetResult(CreateVirtual(), LUse::kAny);
}


void LGen::VisitCall(HIRInstruction* instr) {
  // XXX
}


void LGen::VisitGoto(HIRInstruction* instr) {
  // XXX
}


void LGen::VisitPhi(HIRInstruction* instr) {
  // XXX
}


void LGen::VisitIf(HIRInstruction* instr) {
  // XXX
}

} // namespace internal
} // namespace candor
