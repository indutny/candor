#include "hir-instructions.h"
#include "hir.h"

namespace candor {
namespace internal {

void HIRInstruction::Init(HIRBasicBlock* block, int id) {
  block_ = block;
  id_ = id;
}


void HIRInstruction::Use(HIRValue* value) {
  value->uses()->Push(this);
}


#define HIR_ENUM_INSTRUCTIONS(V)\
    V(None)\
    V(Entry)\
    V(Return)\
    V(Goto)\
    V(StoreLocal)\
    V(StoreContext)\
    V(StoreProperty)\
    V(LoadRoot)\
    V(LoadLocal)\
    V(LoadContext)\
    V(BranchBool)\
    V(AllocateObject)
#define HIR_GEN_SWITCH_CASE(V)\
    case k##V: str = #V; break;


void HIRInstruction::Print(PrintBuffer* p) {
  const char* str;

  switch (type()) {
   HIR_ENUM_INSTRUCTIONS(HIR_GEN_SWITCH_CASE)
   default: str = NULL; break;
  }

  p->Print("[%s", str);
  List<HIRValue*, ZoneObject>::Item* item = values()->head();
  if (item != NULL) p->Print(" ");
  while (item != NULL) {
    item->value()->Print(p);
    item = item->next();
    if (item != NULL) p->Print(" ");
  }
  p->Print("]");
}


#undef HIR_GEN_SWITCH_CASE
#undef HIR_ENUM_INSTRUCTONS


void HIRBranchBase::Init(HIRBasicBlock* block, int id) {
  block->AddSuccessor(left());
  block->AddSuccessor(right());
}


void HIRAllocateObject::Init(HIRBasicBlock* block, int id) {
  SetResult(new HIRValue(block));
}

} // namespace internal
} // namespace candor
