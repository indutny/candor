#include "lir-instructions.h"
#include "hir.h" // HIRValue::Print
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


#define LIR_TYPE_TO_STRING(V)\
    case k##V: str_type = #V; break;

void LIRInstruction::Print(PrintBuffer* p) {
  const char* str_type;
  switch (type()) {
    LIR_ENUM_INSTRUCTIONS(LIR_TYPE_TO_STRING)
    default: UNEXPECTED break;
  }

  p->Print("%d: ", id());

  if (result != NULL) {
    result->Print(p);
    p->Print(" = ");
  }
  p->Print("%s", str_type);

  if (input_count() > 0) {
    p->Print(" [");
    for (int i = 0; i < input_count(); i++) {
      if (i != 0) p->Print(", ");
      inputs[i]->Print(p);
    }
    p->Print("]");
  }

  if (args()->length() > 0) {
    p->Print(" (");
    LIROperandList::Item* item = args()->head();
    for (; item != NULL; item = item->next()) {
      if (item->prev() != NULL) p->Print(", ");
      item->value()->Print(p);
    }
    p->Print(")");
  }
}

#undef LIR_TYPE_TO_STRING

} // namespace internal
} // namespace candor
