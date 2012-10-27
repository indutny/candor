#include "lir.h"
#include "lir-inl.h"
#include "lir-instructions.h"
#include "lir-instructions-inl.h"

namespace candor {
namespace internal {

void LInstruction::Print(PrintBuffer* p) {
  p->Print("%d: ", id);

  if (result) {
    result->Print(p);
    p->Print(" = ");
  }

  p->Print("%s", TypeToStr(type()));
  if (type() == kLiteral && hir()->ast() != NULL) {
    p->Print("[");
    p->PrintValue(hir()->ast()->value(), hir()->ast()->length());
    p->Print("]");
  }

  for (int i = 0; i < input_count(); i++) {
    if (i == 0) p->Print(" ");
    inputs[i]->Print(p);
    if (i + 1 < input_count()) p->Print(", ");
  }

  if (scratch_count()) {
    p->Print(" # scratches: ");
    for (int i = 0; i < scratch_count(); i++) {
      scratches[i]->Print(p);
      if (i + 1 < scratch_count()) p->Print(", ");
    }
  }

  p->Print("\n");
}


void LGap::Resolve() {
}


void LGap::Print(PrintBuffer* p) {
  p->Print("%d: Gap[", id);

  PairList::Item* head;
  if (unhandled_pairs_.length() > 0) {
    assert(pairs_.length() == 0);

    head = unhandled_pairs_.head();
  } else {
    head = pairs_.head();
  }

  for (; head != NULL; head = head->next()) {
    Pair* pair = head->value();
    pair->from_->Print(p);
    p->Print(" => ");
    pair->to_->Print(p);

    if (head->next() != NULL) p->Print(", ");
  }

  p->Print("]\n");
}


} // namespace internal
} // namespace candor
