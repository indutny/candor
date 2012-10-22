#include "hir.h"
#include "hir-inl.h"
#include "hir-instructions.h"
#include "hir-instructions-inl.h"

namespace candor {
namespace internal {
namespace hir {

HInstruction::HInstruction(HGen* g, HBlock* block, Type type) :
    id(g->instr_id()),
    g_(g),
    block_(block),
    type_(type),
    slot_(NULL),
    ast_(NULL),
    removed_(false) {
}


HInstruction::HInstruction(HGen* g,
                          HBlock* block,
                          Type type,
                          ScopeSlot* slot) :
    id(g->instr_id()),
    g_(g),
    block_(block),
    type_(type),
    slot_(slot),
    ast_(NULL),
    removed_(false) {
}


void HInstruction::ReplaceArg(HInstruction* o, HInstruction* n) {
  HInstructionList::Item* head = args()->head();
  for (; head != NULL; head = head->next()) {
    HInstruction* arg = head->value();
    if (arg == o) {
      args()->InsertBefore(head, n);
      args()->Remove(head);

      o->RemoveUse(this);
      n->uses()->Push(this);

      break;
    }
  }
}


void HInstruction::RemoveUse(HInstruction* i) {
  HInstructionList::Item* head = uses()->head();
  for (; head != NULL; head = head->next()) {
    HInstruction* use = head->value();
    if (use == i) {
      uses()->Remove(head);
      break;
    }
  }
}


void HInstruction::Print(PrintBuffer* p) {
  p->Print("i%d = ", id);

  p->Print("%s", TypeToStr(type_));

  if (ast() != NULL && ast()->value() != NULL) {
    p->Print("[");
    p->PrintValue(ast()->value(), ast()->length());
    p->Print("]");
  }

  if (args()->length() == 0) {
    p->Print("\n");
    return;
  }

  HInstructionList::Item* head = args()->head();
  p->Print("(");
  for (; head != NULL; head = head->next()) {
    p->Print("i%d", head->value()->id);
    if (head->next() != NULL) p->Print(", ");
  }
  p->Print(")\n");
}


HPhi::HPhi(HGen* g, HBlock* block, ScopeSlot* slot) :
    HInstruction(g, block, kPhi, slot),
    input_count_(0) {
  inputs_[0] = NULL;
  inputs_[1] = NULL;

  block->env()->Set(slot, this);
  block->env()->SetPhi(slot, this);
}


HFunction::HFunction(HGen* g, HBlock* block, AstNode* ast) :
    HInstruction(g, block, kFunction),
    body(NULL) {
  ast_ = ast;
}


void HFunction::Print(PrintBuffer* p) {
  p->Print("i%d = Function[b%d]\n", id, body->id);
}

} // namespace hir
} // namespace internal
} // namespace candor
