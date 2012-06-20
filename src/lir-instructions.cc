#include "lir-instructions.h"
#include "hir.h" // HIRValue::Print
#include "hir-instructions.h" // HIRParallelMove
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

  if (type() == LIRInstruction::kParallelMove) {
    ZoneList<HIRParallelMove::MoveItem*>::Item* item =
        HIRParallelMove::Cast(generic_hir())->moves()->head();

    p->Print(" ");
    for (; item != NULL; item = item->next()) {
      HIRParallelMove::MoveItem* move = item->value();
      move->source()->Print(p);
      p->Print("=>");
      move->target()->Print(p);
      if (item->next() != NULL) p->Print(",");
    }
  }

  if (input_count() > 0) {
    p->Print(" [");
    for (int i = 0; i < input_count(); i++) {
      if (i != 0) p->Print(", ");
      if (inputs[i] != NULL) inputs[i]->Print(p);
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
