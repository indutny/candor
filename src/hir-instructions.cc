#include "hir.h"
#include "hir-inl.h"
#include "hir-instructions.h"
#include "hir-instructions-inl.h"

namespace candor {
namespace internal {
namespace hir {

Instruction::Instruction(Gen* g, Block* block, InstructionType type) :
    id(g->instr_id()),
    g_(g),
    block_(block),
    type_(type),
    slot_(NULL),
    prev_(NULL),
    next_(NULL) {
}


Instruction::Instruction(Gen* g,
                         Block* block,
                         InstructionType type,
                         ScopeSlot* slot) :
    id(g->instr_id()),
    g_(g),
    block_(block),
    type_(type),
    slot_(slot),
    prev_(NULL),
    next_(NULL) {
}


void Instruction::Print(PrintBuffer* p) {
  p->Print("i%d = %s", id, TypeToStr(type_));
  if (args_.length() == 0) {
    p->Print("\n");
    return;
  }

  InstructionList::Item* head = args_.head();
  p->Print("(");
  for (; head != NULL; head = head->next()) {
    p->Print("i%d", head->value()->id);
    if (head->next() != NULL) p->Print(", ");
  }
  p->Print(")\n");
}


Phi::Phi(Gen* g, Block* block, ScopeSlot* slot) :
    Instruction(g, block, kPhi, slot),
    value_count_(0) {
  values_[0] = NULL;
  values_[1] = NULL;

  block->AddPhi(slot, this);
}


Function::Function(Gen* g, Block* block, AstNode* ast) :
    Instruction(g, block, kFunction),
    body(NULL),
    ast_(ast) {
}

} // namespace hir
} // namespace internal
} // namespace candor
