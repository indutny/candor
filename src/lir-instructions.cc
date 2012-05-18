#include "lir-instructions.h"
#include "macroassembler.h"
#include "macroassembler-inl.h"

namespace candor {
namespace internal {

void LIRInstruction::AddUse(RelocationInfo* info) {
  if (relocated()) {
    info->target(relocation_offset_);
    masm_->relocation_info_.Push(info);
    return;
  }
  uses()->Push(info);
}


void LIRInstruction::Relocate(Masm* masm) {
  if (relocated()) return;
  masm_ = masm;
  relocation_offset_ = masm->offset();
  relocated(true);

  RelocationInfo* block_reloc;
  while ((block_reloc = uses()->Shift()) != NULL) {
    block_reloc->target(masm->offset());
    masm->relocation_info_.Push(block_reloc);
  }
}


void LIRInstruction::AddOperand(LIROperand* operand) {
  // Remove operands with same HIRValue
  LIROperandList::Item* op = operands()->tail();
  for (; op != NULL; op = op->prev()) {
    if (op->value()->hir() == operand->hir()) operands()->Remove(op);
  }

  // Push new one
  operands()->Push(operand);
}

} // namespace internal
} // namespace candor
