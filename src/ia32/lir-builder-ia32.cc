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
  Bind(new LNil())
      ->SetResult(CreateVirtual(), LUse::kAny);
}


void LGen::VisitEntry(HIRInstruction* instr) {
  Bind(new LEntry(HIREntry::Cast(instr)->context_slots()));
}


void LGen::VisitReturn(HIRInstruction* instr) {
  Bind(new LReturn())
      ->AddArg(ToFixed(instr->left(), eax), LUse::kRegister);
}


void LGen::VisitLiteral(HIRInstruction* instr) {
  Bind(new LLiteral(HIRLiteral::Cast(instr)->root_slot()))
      ->SetResult(CreateVirtual(), LUse::kAny);
}


void LGen::VisitAllocateObject(HIRInstruction* instr) {
  LInstruction* op = Bind(new LAllocateObject())->MarkHasCall();

  ResultFromFixed(op, eax);
}


void LGen::VisitAllocateArray(HIRInstruction* instr) {
  LInstruction* op = Bind(new LAllocateArray())->MarkHasCall();

  ResultFromFixed(op, eax);
}


void LGen::VisitFunction(HIRInstruction* instr) {
  HIRFunction* fn = HIRFunction::Cast(instr);

  // Generate block and label if needed
  if (fn->body->lir() == NULL) new LBlock(fn->body);

  // XXX : Store address somewhere
  LInstruction* op = Bind(new LFunction(fn->body->lir(), fn->arg_count))
      ->MarkHasCall()
      ->AddScratch(CreateVirtual());

  ResultFromFixed(op, eax);
}


void LGen::VisitNot(HIRInstruction* instr) {
  LInstruction* op = Bind(new LNot())
      ->MarkHasCall()
      ->AddArg(ToFixed(instr->left(), eax), LUse::kRegister);

  ResultFromFixed(op, eax);
}


void LGen::VisitBinOp(HIRInstruction* instr) {
  LInstruction* op = Bind(new LBinOp())
      ->MarkHasCall()
      ->AddArg(ToFixed(instr->left(), eax), LUse::kRegister)
      ->AddArg(ToFixed(instr->right(), ebx), LUse::kRegister);

  ResultFromFixed(op, eax);
}


void LGen::VisitSizeof(HIRInstruction* instr) {
  LInstruction* op = Bind(new LSizeof())
      ->MarkHasCall()
      ->AddArg(ToFixed(instr->left(), eax), LUse::kRegister);

  ResultFromFixed(op, eax);
}


void LGen::VisitTypeof(HIRInstruction* instr) {
  LInstruction* op = Bind(new LTypeof())
      ->MarkHasCall()
      ->AddArg(ToFixed(instr->left(), eax), LUse::kRegister);

  ResultFromFixed(op, eax);
}


void LGen::VisitKeysof(HIRInstruction* instr) {
  LInstruction* op = Bind(new LKeysof())
      ->MarkHasCall()
      ->AddArg(ToFixed(instr->left(), eax), LUse::kRegister);

  ResultFromFixed(op, eax);
}


void LGen::VisitClone(HIRInstruction* instr) {
  LInstruction* op = Bind(new LClone())
      ->MarkHasCall()
      ->AddArg(ToFixed(instr->left(), eax), LUse::kRegister);

  ResultFromFixed(op, eax);
}


void LGen::VisitLoadContext(HIRInstruction* instr) {
  Bind(new LLoadContext())
      ->SetSlot(HIRLoadContext::Cast(instr)->context_slot())
      ->SetResult(CreateVirtual(), LUse::kRegister);
}


void LGen::VisitStoreContext(HIRInstruction* instr) {
  Bind(new LStoreContext())
      ->SetSlot(HIRStoreContext::Cast(instr)->context_slot())
      ->AddScratch(CreateVirtual())
      ->AddArg(instr->left(), LUse::kRegister);
}


void LGen::VisitLoadProperty(HIRInstruction* instr) {
  LInstruction* load = Bind(new LLoadProperty())
      ->MarkHasCall()
      ->AddArg(ToFixed(instr->left(), eax), LUse::kRegister)
      ->AddArg(ToFixed(instr->right(), ebx), LUse::kRegister);

  ResultFromFixed(load, eax);
}


void LGen::VisitStoreProperty(HIRInstruction* instr) {
  LInstruction* load = Bind(new LStoreProperty())
      ->MarkHasCall()
      ->AddScratch(CreateVirtual())
      ->AddArg(ToFixed(instr->left(), eax), LUse::kRegister)
      ->AddArg(ToFixed(instr->right(), ebx), LUse::kRegister)
      ->SetResult(ToFixed(instr->third(), ecx), LUse::kRegister);
}


void LGen::VisitDeleteProperty(HIRInstruction* instr) {
  Bind(new LDeleteProperty())
      ->MarkHasCall()
      ->AddArg(ToFixed(instr->left(), eax), LUse::kRegister)
      ->AddArg(ToFixed(instr->right(), ebx), LUse::kRegister);
}


void LGen::VisitGetStackTrace(HIRInstruction* instr) {
  LInstruction* trace = Bind(new LGetStackTrace())
      ->MarkHasCall();

  ResultFromFixed(trace, eax);
}


void LGen::VisitCollectGarbage(HIRInstruction* instr) {
  Bind(new LCollectGarbage())
      ->MarkHasCall();
}


void LGen::VisitLoadArg(HIRInstruction* instr) {
  Bind(new LLoadArg())
      ->AddArg(instr->left(), LUse::kRegister)
      ->SetResult(CreateVirtual(), LUse::kAny);
}


void LGen::VisitLoadVarArg(HIRInstruction* instr) {
  Bind(new LLoadVarArg())
      ->MarkHasCall()
      ->AddArg(ToFixed(instr->left(), eax), LUse::kRegister)
      ->AddArg(ToFixed(instr->right(), ebx), LUse::kRegister)
      ->SetResult(ToFixed(instr->third(), ecx), LUse::kAny);
}


void LGen::VisitStoreArg(HIRInstruction* instr) {
  Bind(new LStoreArg())
      ->AddArg(instr->left(), LUse::kRegister);
}


void LGen::VisitAlignStack(HIRInstruction* instr) {
  Bind(new LAlignStack())
      ->AddScratch(CreateVirtual())
      ->AddArg(instr->left(), LUse::kRegister);
}


void LGen::VisitStoreVarArg(HIRInstruction* instr) {
  Bind(new LStoreVarArg())
      ->MarkHasCall()
      ->AddScratch(CreateVirtual())
      ->AddArg(ToFixed(instr->left(), eax), LUse::kRegister);
}


void LGen::VisitCall(HIRInstruction* instr) {
  LInstruction* op = Bind(new LCall())
      ->MarkHasCall()
      ->AddArg(ToFixed(instr->left(), ebx), LUse::kRegister)
      ->AddArg(ToFixed(instr->right(), eax), LUse::kRegister);

  ResultFromFixed(op, eax);
}


void LGen::VisitIf(HIRInstruction* instr) {
  assert(instr->block()->succ_count() == 2);
  Bind(new LBranch())
      ->MarkHasCall()
      ->AddArg(ToFixed(instr->left(), eax), LUse::kRegister);
}


} // namespace internal
} // namespace candor
