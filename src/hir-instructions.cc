#include "hir-instructions.h"
#include "hir.h"

namespace candor {
namespace internal {

void HIRInstruction::Init(HIRBasicBlock* block) {
  block_ = block;
}


void HIRInstruction::Use(HIRValue* value) {
  value->uses()->Push(this);
}


void HIRInstruction::Print(PrintBuffer* p) {
  const char* str;

#define ENUM_TYPE(V)\
    V(None)\
    V(Goto)\
    V(StoreLocal)\
    V(StoreContext)\
    V(StoreProperty)\
    V(LoadRoot)\
    V(LoadLocal)\
    V(LoadContext)\
    V(BranchBool)\
    V(AllocateObject)
#define SWITCH_CASE(V)\
    case k##V: str = #V; break;

  switch (type()) {
   ENUM_TYPE(SWITCH_CASE)
   default: str = NULL; break;
  }

#undef SWITCH_CASE
#undef ENUM_TYPE

  p->Print("[%s ", str);
  List<HIRValue*, ZoneObject>::Item* item = values()->head();
  while (item != NULL) {
    item->value()->Print(p);
    item = item->next();
    if (item != NULL) p->Print(" ");
  }
  p->Print("]");
}


void HIRBranchBase::Init(HIRBasicBlock* block) {
  block->AddSuccessor(left());
  block->AddSuccessor(right());
}


void HIRAllocateObject::Init(HIRBasicBlock* block) {
  SetResult(new HIRValue(block));
}

} // namespace internal
} // namespace candor
