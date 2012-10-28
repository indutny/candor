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
      ->AddArg(ToFixed(instr->left(), rax), LUse::kRegister);
}


void LGen::VisitLiteral(HIRInstruction* instr) {
  Bind(new LLiteral(HIRLiteral::Cast(instr)->root_slot()))
      ->SetResult(CreateVirtual(), LUse::kAny);
}


void LGen::VisitAllocateObject(HIRInstruction* instr) {
  Bind(new LAllocateObject())
      ->MarkHasCall()
      ->SetResult(CreateVirtual(), LUse::kRegister);
}


void LGen::VisitAllocateArray(HIRInstruction* instr) {
  Bind(new LAllocateArray())
      ->MarkHasCall()
      ->SetResult(CreateVirtual(), LUse::kRegister);
}


void LGen::VisitFunction(HIRInstruction* instr) {
  HIRFunction* fn = HIRFunction::Cast(instr);

  // XXX : Store address somewhere
  Bind(new LFunction(fn->body->lir()))
      ->MarkHasCall()
      ->SetResult(CreateVirtual(), LUse::kRegister);
}


void LGen::VisitNot(HIRInstruction* instr) {
  LInstruction* op = Bind(new LNot())
      ->MarkHasCall()
      ->AddArg(ToFixed(instr->left(), rax), LUse::kRegister);

  ResultFromFixed(op, rax);
}


void LGen::VisitBinOp(HIRInstruction* instr) {
  LInstruction* op = Bind(new LBinOp())
      ->MarkHasCall()
      ->AddArg(ToFixed(instr->left(), rax), LUse::kRegister)
      ->AddArg(ToFixed(instr->right(), rbx), LUse::kRegister);

  ResultFromFixed(op, rax);
}


void LGen::VisitSizeof(HIRInstruction* instr) {
  Bind(new LSizeof())
      ->MarkHasCall()
      ->AddArg(ToFixed(instr->left(), rax), LUse::kRegister)
      ->SetResult(CreateVirtual(), LUse::kRegister);
}


void LGen::VisitTypeof(HIRInstruction* instr) {
  Bind(new LTypeof())
      ->MarkHasCall()
      ->AddArg(ToFixed(instr->left(), rax), LUse::kRegister)
      ->SetResult(CreateVirtual(), LUse::kRegister);
}


void LGen::VisitKeysof(HIRInstruction* instr) {
  Bind(new LKeysof())
      ->MarkHasCall()
      ->AddArg(ToFixed(instr->left(), rax), LUse::kRegister)
      ->SetResult(CreateVirtual(), LUse::kRegister);
}


void LGen::VisitClone(HIRInstruction* instr) {
  Bind(new LClone())
      ->MarkHasCall()
      ->AddArg(ToFixed(instr->left(), rax), LUse::kRegister)
      ->SetResult(CreateVirtual(), LUse::kRegister);
}


void LGen::VisitLoadContext(HIRInstruction* instr) {
  Bind(new LLoadContext())
      ->SetSlot(instr->slot())
      ->SetResult(CreateVirtual(), LUse::kRegister);
}


void LGen::VisitStoreContext(HIRInstruction* instr) {
  Bind(new LStoreContext())
      ->SetSlot(instr->slot())
      ->AddArg(instr->left(), LUse::kRegister)
      ->SetResult(CreateVirtual(), LUse::kRegister);
}


void LGen::VisitLoadProperty(HIRInstruction* instr) {
  LInstruction* store = Bind(new LLoadProperty())
      ->MarkHasCall()
      ->AddScratch(CreateVirtual())
      ->AddArg(ToFixed(instr->left(), rax), LUse::kRegister)
      ->AddArg(ToFixed(instr->right(), rbx), LUse::kRegister)
      ->SetResult(CreateVirtual(), LUse::kRegister);
}


void LGen::VisitStoreProperty(HIRInstruction* instr) {
  LInstruction* load = Bind(new LStoreProperty())
      ->MarkHasCall()
      ->AddArg(ToFixed(instr->left(), rax), LUse::kRegister)
      ->AddArg(ToFixed(instr->right(), rbx), LUse::kRegister)
      ->SetResult(ToFixed(instr->third(), rcx), LUse::kRegister);

  load->Propagate(instr->third());
}


void LGen::VisitDeleteProperty(HIRInstruction* instr) {
  Bind(new LDeleteProperty())
      ->MarkHasCall()
      ->AddArg(ToFixed(instr->left(), rax), LUse::kRegister)
      ->AddArg(ToFixed(instr->right(), rbx), LUse::kRegister);
}


void LGen::VisitGetStackTrace(HIRInstruction* instr) {
  Bind(new LGetStackTrace())
      ->MarkHasCall();
}


void LGen::VisitCollectGarbage(HIRInstruction* instr) {
  Bind(new LCollectGarbage())
      ->MarkHasCall();
}


void LGen::VisitLoadArg(HIRInstruction* instr) {
  Bind(new LLoadArg(HIRLoadArg::Cast(instr)->index()))
      ->SetResult(CreateVirtual(), LUse::kAny);
}


void LGen::VisitCall(HIRInstruction* instr) {
  // XXX
}


void LGen::VisitGoto(HIRInstruction* instr) {
  HIRBlock* succ = instr->block()->SuccAt(0);
  int parent_index = succ->PredAt(0) != instr->block();

  HIRPhiList::Item* head = succ->phis()->head();
  for (; head != NULL; head = head->next()) {
    HIRPhi* phi = head->value();
    LInstruction* lphi = NULL;

    // Initialize LIR representation of phi
    if (phi->lir() == NULL) {
      lphi = new LPhi();
      lphi->AddArg(CreateVirtual(), LUse::kAny);

      phi->lir(lphi);
    } else {
      lphi = phi->lir();
    }
    assert(lphi != NULL);

    Add(new LMove())
        ->SetResult(lphi->inputs[0]->interval(), LUse::kAny)
        ->AddArg(phi->InputAt(parent_index), LUse::kAny);
  }

  Bind(new LGoto());
}


void LGen::VisitPhi(HIRInstruction* instr) {
  Bind(instr->lir())
      ->SetResult(CreateVirtual(), LUse::kAny);
}


void LGen::VisitIf(HIRInstruction* instr) {
  assert(instr->block()->succ_count() == 2);
  Bind(new LBranch())
      ->MarkHasCall()
      ->AddArg(ToFixed(instr->left(), rax), LUse::kRegister);
}


} // namespace internal
} // namespace candor
