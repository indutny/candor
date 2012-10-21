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
    removed_(false),
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


void Instruction::ReplaceArg(Instruction* o, Instruction* n) {
  InstructionList::Item* head = args()->head();
  for (; head != NULL; head = head->next()) {
    Instruction* arg = head->value();
    if (arg == o) {
      args()->InsertBefore(head, n);
      args()->Remove(head);

      o->RemoveUse(this);
      n->uses()->Push(this);

      break;
    }
  }
}


void Instruction::RemoveUse(Instruction* i) {
  InstructionList::Item* head = uses()->head();
  for (; head != NULL; head = head->next()) {
    Instruction* use = head->value();
    if (use == i) {
      uses()->Remove(head);
      break;
    }
  }
}


void Instruction::Print(PrintBuffer* p) {
  p->Print("i%d = %s", id, TypeToStr(type_));
  if (args()->length() == 0) {
    p->Print("\n");
    return;
  }

  InstructionList::Item* head = args()->head();
  p->Print("(");
  for (; head != NULL; head = head->next()) {
    p->Print("i%d", head->value()->id);
    if (head->next() != NULL) p->Print(", ");
  }
  p->Print(")\n");
}


Phi::Phi(Gen* g, Block* block, ScopeSlot* slot) :
    Instruction(g, block, kPhi, slot),
    input_count_(0) {
  inputs_[0] = NULL;
  inputs_[1] = NULL;

  block->env()->Set(slot, this);
  block->env()->SetPhi(slot, this);
}


Function::Function(Gen* g, Block* block, AstNode* ast) :
    Instruction(g, block, kFunction),
    body(NULL),
    ast_(ast) {
}

} // namespace hir
} // namespace internal
} // namespace candor
