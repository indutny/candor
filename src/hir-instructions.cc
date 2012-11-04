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
    removed_(false),
    representation_(kHoleRepresentation) {
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
    removed_(false),
    representation_(kHoleRepresentation) {
}


inline void HIRInstruction::CalculateRepresentation() {
  representation_ = kUnknownRepresentation;
}


void HIRInstruction::ReplaceArg(HIRInstruction* o, HIRInstruction* n) {
  HIRInstructionList::Item* head = args()->head();
  HIRInstructionList::Item* next;
  for (; head != NULL; head = next) {
    HIRInstruction* arg = head->value();
    next = head->next();

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
  HIRInstructionList::Item* next;
  for (; head != NULL; head = next) {
    HIRInstruction* use = head->value();
    next = head->next();

    if (use == i) {
      uses()->Remove(head);
      break;
    }
  }
}


void HIRInstruction::Print(PrintBuffer* p) {
  p->Print("i%d = ", id);

  p->Print("%s", TypeToStr(type_));

  if (type() == HIRInstruction::kLiteral &&
      ast() != NULL && ast()->value() != NULL) {
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


void HIRPhi::CalculateRepresentation() {
  int result = kAnyRepresentation;

  for (int i = 0; i < input_count_; i++) {
    result = result & inputs_[i]->representation();
  }

  representation_ = static_cast<Representation>(result);
}


void HIRPhi::ReplaceArg(HIRInstruction* o, HIRInstruction* n) {
  HIRInstruction::ReplaceArg(o, n);

  for (int i = 0; i < input_count(); i++) {
    if (inputs_[i] == o) inputs_[i] = n;
  }
}


HIRLiteral::HIRLiteral(HIRGen* g,
                       HIRBlock* block,
                       AstNode::Type type,
                       ScopeSlot* slot) :
    HIRInstruction(g, block, kLiteral),
    type_(type),
    root_slot_(slot) {
}


void HIRLiteral::CalculateRepresentation() {
  switch (type_) {
   case AstNode::kNumber:
    representation_ = kNumberRepresentation;
    break;
   case AstNode::kString:
   case AstNode::kProperty:
    representation_ = kStringRepresentation;
    break;
   case AstNode::kTrue:
   case AstNode::kFalse:
    representation_ = kBooleanRepresentation;
    break;
   default:
    representation_ = kUnknownRepresentation;
  }
}


HIRFunction::HIRFunction(HIRGen* g, HIRBlock* block, AstNode* ast) :
    HIRInstruction(g, block, kFunction),
    body(NULL),
    arg_count(0) {
  ast_ = ast;
}


void HIRFunction::CalculateRepresentation() {
  representation_ = kFunctionRepresentation;
}


void HIRFunction::Print(PrintBuffer* p) {
  p->Print("i%d = Function[b%d]\n", id, body->id);
}


HIREntry::HIREntry(HIRGen* g, HIRBlock* block, int context_slots_) :
    HIRInstruction(g, block, kEntry),
    context_slots_(context_slots_) {
}


void HIREntry::Print(PrintBuffer* p) {
  p->Print("i%d = Entry[%d]\n", id, context_slots_);
}


HIRBinOp::HIRBinOp(HIRGen* g, HIRBlock* block, BinOp::BinOpType type) :
    HIRInstruction(g, block, kBinOp),
    binop_type_(type) {
}


void HIRBinOp::CalculateRepresentation() {
  int left = args()->head()->value()->representation();
  int right = args()->tail()->value()->representation();
  int res;

  if (BinOp::is_binary(binop_type_)) {
    res = kSmiRepresentation;
  } else if (BinOp::is_logic(binop_type_)) {
    res = kBooleanRepresentation;
  } else if (BinOp::is_math(binop_type_)) {
    if (binop_type_ != BinOp::kAdd) {
      res = kNumberRepresentation;
    } else if ((left | right) & kStringRepresentation) {
      // "123" + any, or any + "123"
      res = kStringRepresentation;
    } else {
      int mask = kSmiRepresentation |
                 kHeapNumberRepresentation |
                 kNilRepresentation;
      res = left & right & mask;
    }
  } else {
    res = kUnknownRepresentation;
  }

  representation_ = static_cast<Representation>(res);
}


HIRLoadContext::HIRLoadContext(HIRGen* g, HIRBlock* block, ScopeSlot* slot) :
    HIRInstruction(g, block, kLoadContext),
    context_slot_(slot) {
}


HIRStoreContext::HIRStoreContext(HIRGen* g, HIRBlock* block, ScopeSlot* slot) :
    HIRInstruction(g, block, kStoreContext),
    context_slot_(slot) {
}

} // namespace internal
} // namespace candor
