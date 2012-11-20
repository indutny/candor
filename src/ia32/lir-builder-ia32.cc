/**
 * Copyright (c) 2012, Fedor Indutny.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
      ->SetResult(CreateConst(), LUse::kAny);
}


void LGen::VisitEntry(HIRInstruction* instr) {
  HIREntry* entry = HIREntry::Cast(instr);
  Bind(new LEntry(entry->label(), entry->context_slots()));
}


void LGen::VisitReturn(HIRInstruction* instr) {
  LInterval* lhs = ToFixed(instr->left(), eax);
  Bind(new LReturn())
      ->AddArg(lhs, LUse::kRegister);
}


void LGen::VisitLiteral(HIRInstruction* instr) {
  Bind(new LLiteral(HIRLiteral::Cast(instr)->root_slot()))
      ->SetResult(CreateConst(), LUse::kAny);
}


void LGen::VisitAllocateObject(HIRInstruction* instr) {
  HIRAllocateObject* obj = HIRAllocateObject::Cast(instr);
  LInstruction* op = Bind(new LAllocateObject(obj->size()))->MarkHasCall();

  ResultFromFixed(op, eax);
}


void LGen::VisitAllocateArray(HIRInstruction* instr) {
  HIRAllocateArray* arr = HIRAllocateArray::Cast(instr);
  LInstruction* op = Bind(new LAllocateArray(arr->size()))->MarkHasCall();

  ResultFromFixed(op, eax);
}


void LGen::VisitFunction(HIRInstruction* instr) {
  HIRFunction* fn = HIRFunction::Cast(instr);

  LInstruction* op = Bind(new LFunction(fn->ast()->label(), fn->arg_count))
      ->MarkHasCall()
      ->AddScratch(CreateVirtual());

  ResultFromFixed(op, eax);
}


void LGen::VisitNot(HIRInstruction* instr) {
  LInterval* lhs = ToFixed(instr->left(), eax);
  LInstruction* op = Bind(new LNot())
      ->MarkHasCall()
      ->AddArg(lhs, LUse::kRegister);

  ResultFromFixed(op, eax);
}


void LGen::VisitBinOp(HIRInstruction* instr) {
  LInstruction* op;
  LInterval* lhs = ToFixed(instr->left(), eax);
  LInterval* rhs = ToFixed(instr->right(), ebx);
  HIRBinOp* hir = HIRBinOp::Cast(instr);

  if (instr->right()->IsNumber() && instr->left()->IsNumber() &&
      BinOp::is_math(hir->binop_type()) && hir->binop_type() != BinOp::kDiv) {
    op = Bind(new LBinOpNumber())
        ->MarkHasCall()
        ->AddScratch(CreateVirtual())
        ->AddArg(lhs, LUse::kRegister)
        ->AddArg(rhs, LUse::kRegister);
  } else {
    op = Bind(new LBinOp())
        ->MarkHasCall()
        ->AddArg(lhs, LUse::kRegister)
        ->AddArg(rhs, LUse::kRegister);
  }

  ResultFromFixed(op, eax);
}


void LGen::VisitSizeof(HIRInstruction* instr) {
  LInterval* lhs = ToFixed(instr->left(), eax);
  LInstruction* op = Bind(new LSizeof())
      ->MarkHasCall()
      ->AddArg(lhs, LUse::kRegister);

  ResultFromFixed(op, eax);
}


void LGen::VisitTypeof(HIRInstruction* instr) {
  LInterval* lhs = ToFixed(instr->left(), eax);
  LInstruction* op = Bind(new LTypeof())
      ->MarkHasCall()
      ->AddArg(lhs, LUse::kRegister);

  ResultFromFixed(op, eax);
}


void LGen::VisitKeysof(HIRInstruction* instr) {
  LInterval* lhs = ToFixed(instr->left(), eax);
  LInstruction* op = Bind(new LKeysof())
      ->MarkHasCall()
      ->AddArg(lhs, LUse::kRegister);

  ResultFromFixed(op, eax);
}


void LGen::VisitClone(HIRInstruction* instr) {
  LInterval* lhs = ToFixed(instr->left(), eax);
  LInstruction* op = Bind(new LClone())
      ->MarkHasCall()
      ->AddArg(lhs, LUse::kRegister);

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
  LInterval* lhs = ToFixed(instr->left(), eax);
  LInterval* rhs = ToFixed(instr->right(), ebx);
  LLoadProperty* load = new LLoadProperty();

  Bind(load)
      ->MarkHasCall()
      ->AddArg(lhs, LUse::kRegister)
      ->AddArg(rhs, LUse::kRegister);

  if (instr->right()->Is(HIRInstruction::kLiteral)) {
    load->SetMonomorphicProperty();
  }

  ResultFromFixed(load, eax);
}


void LGen::VisitStoreProperty(HIRInstruction* instr) {
  LInterval* lhs = ToFixed(instr->left(), eax);
  LInterval* rhs = ToFixed(instr->right(), ebx);
  ToFixed(instr->third(), ecx);
  LStoreProperty* store = new LStoreProperty();
  Bind(store)
      ->MarkHasCall()
      ->AddArg(lhs, LUse::kRegister)
      ->AddArg(rhs, LUse::kRegister);

  if (instr->right()->Is(HIRInstruction::kLiteral)) {
    store->SetMonomorphicProperty();
  }
}


void LGen::VisitDeleteProperty(HIRInstruction* instr) {
  LInterval* lhs = ToFixed(instr->left(), eax);
  LInterval* rhs = ToFixed(instr->right(), ebx);
  LInstruction* del = Bind(new LDeleteProperty())
      ->MarkHasCall()
      ->AddArg(lhs, LUse::kRegister)
      ->AddArg(rhs, LUse::kRegister);
  ResultFromFixed(del, eax);
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
  LInterval* lhs = ToFixed(instr->left(), eax);
  LInterval* rhs = ToFixed(instr->right(), ebx);
  LInterval* third = ToFixed(instr->third(), ecx);
  Bind(new LLoadVarArg())
      ->MarkHasCall()
      ->AddArg(lhs, LUse::kRegister)
      ->AddArg(rhs, LUse::kRegister)
      ->SetResult(third, LUse::kAny);
}


void LGen::VisitStoreArg(HIRInstruction* instr) {
  Bind(new LStoreArg())
      ->AddArg(instr->left(), LUse::kRegister)
      ->AddArg(instr->right(), LUse::kRegister);
}


void LGen::VisitAlignStack(HIRInstruction* instr) {
  Bind(new LAlignStack())
      ->AddScratch(CreateVirtual())
      ->AddArg(instr->left(), LUse::kRegister);
}


void LGen::VisitStoreVarArg(HIRInstruction* instr) {
  LInterval* lhs = ToFixed(instr->left(), eax);
  LInterval* rhs = ToFixed(instr->right(), ebx);
  Bind(new LStoreVarArg())
      ->MarkHasCall()
      ->AddScratch(CreateVirtual())
      ->AddArg(lhs, LUse::kRegister)
      ->AddArg(rhs, LUse::kRegister);
}


void LGen::VisitCall(HIRInstruction* instr) {
  LInterval* lhs = ToFixed(instr->left(), ebx);
  LInterval* rhs = ToFixed(instr->right(), eax);
  LInstruction* op = Bind(new LCall())
      ->MarkHasCall()
      ->AddArg(lhs, LUse::kRegister)
      ->AddArg(rhs, LUse::kRegister);

  ResultFromFixed(op, eax);
}


void LGen::VisitIf(HIRInstruction* instr) {
  if (instr->left()->IsNumber()) {
    Bind(new LBranchNumber())
        ->AddArg(instr->left(), LUse::kRegister);
    return;
  }

  LInterval* lhs = ToFixed(instr->left(), eax);
  assert(instr->block()->succ_count() == 2);
  Bind(new LBranch())
      ->MarkHasCall()
      ->AddArg(lhs, LUse::kRegister);
}


}  // namespace internal
}  // namespace candor
