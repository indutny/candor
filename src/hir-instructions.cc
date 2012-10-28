#include "hir.h"
#include "hir-inl.h"
#include "hir-instructions.h"
#include "hir-instructions-inl.h"

namespace candor {
namespace internal {

HIRInstruction::HIRInstruction(HIRGen* g, HIRBlock* block, Type type) :
    id(g->instr_id()),
    g_(g),
    block_(block),
    type_(type),
    slot_(NULL),
    ast_(NULL),
    lir_(NULL),
    removed_(false) {
}


HIRInstruction::HIRInstruction(HIRGen* g,
                               HIRBlock* block,
                               Type type,
                               ScopeSlot* slot) :
    id(g->instr_id()),
    g_(g),
    block_(block),
    type_(type),
    slot_(slot),
    ast_(NULL),
    lir_(NULL),
    removed_(false) {
}


void HIRInstruction::ReplaceArg(HIRInstruction* o, HIRInstruction* n) {
  HIRInstructionList::Item* head = args()->head();
  for (; head != NULL; head = head->next()) {
    HIRInstruction* arg = head->value();
    if (arg == o) {
      args()->InsertBefore(head, n);
      args()->Remove(head);

      o->RemoveUse(this);
      n->uses()->Push(this);

      break;
    }
  }
}


void HIRInstruction::RemoveUse(HIRInstruction* i) {
  HIRInstructionList::Item* head = uses()->head();
  for (; head != NULL; head = head->next()) {
    HIRInstruction* use = head->value();
    if (use == i) {
      uses()->Remove(head);
      break;
    }
  }
}


void HIRInstruction::Print(PrintBuffer* p) {
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

  HIRInstructionList::Item* head = args()->head();
  p->Print("(");
  for (; head != NULL; head = head->next()) {
    p->Print("i%d", head->value()->id);
    if (head->next() != NULL) p->Print(", ");
  }
  p->Print(")\n");
}


HIRPhi::HIRPhi(HIRGen* g, HIRBlock* block, ScopeSlot* slot) :
    HIRInstruction(g, block, kPhi, slot),
    input_count_(0) {
  inputs_[0] = NULL;
  inputs_[1] = NULL;

  block->env()->Set(slot, this);
  block->env()->SetPhi(slot, this);
}


HIRFunction::HIRFunction(HIRGen* g, HIRBlock* block, AstNode* ast) :
    HIRInstruction(g, block, kFunction),
    body(NULL) {
  ast_ = ast;
}


void HIRFunction::Print(PrintBuffer* p) {
  p->Print("i%d = Function[b%d]\n", id, body->id);
}


HIRLoadArg::HIRLoadArg(HIRGen* g, HIRBlock* block, int index) :
    HIRInstruction(g, block, kLoadArg),
    index_(index) {
}


void HIRLoadArg::Print(PrintBuffer* p) {
  p->Print("i%d = LoadArg[%d]\n", id, index_);
}


HIREntry::HIREntry(HIRGen* g, HIRBlock* block, int context_slots_) :
    HIRInstruction(g, block, kEntry),
    context_slots_(context_slots_) {
}


void HIREntry::Print(PrintBuffer* p) {
  p->Print("i%d = Entry[%d]\n", id, context_slots_);
}

} // namespace internal
} // namespace candor
