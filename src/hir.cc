#include "hir.h"
#include "hir-inl.h"

namespace candor {
namespace internal {
namespace hir {

Gen::Gen(Heap* heap, AstNode* root) : Visitor<Instruction>(kPreorder),
                                      current_block_(NULL),
                                      current_root_(NULL),
                                      root_(heap),
                                      block_id_(0),
                                      instr_id_(0) {
  work_queue_.Push(new Function(this, NULL, root));

  while (work_queue_.length() != 0) {
    Block* b = CreateBlock();
    set_current_block(b);
    set_current_root(b);

    Function* current = Function::Cast(work_queue_.Shift());
    current->body = b;
    Visit(current->ast());
  }
}


Instruction* Gen::VisitFunction(AstNode* stmt) {
  FunctionLiteral* fn = FunctionLiteral::Cast(stmt);

  if (current_root() == current_block() &&
      current_block()->IsEmpty()) {
    Instruction* entry = CreateInstruction(kEntry);

    AstList::Item* args_head = fn->args()->head();
    for (; args_head != NULL; args_head = args_head->next()) {
      entry->AddArg(Visit(args_head->value()));
    }

    Add(entry);

    VisitChildren(stmt);

    if (!current_block()->IsEnded()) {
      Instruction* val = Add(kNil);
      Instruction* end = Return(kReturn);
      end->AddArg(val);
    }

    return NULL;
  } else {
    Function* f = new Function(this, current_block(), stmt);
    work_queue_.Push(f);
    return Add(f);
  }
}


void Gen::Assign(ScopeSlot* slot, Instruction* value) {
  slot->hir(value);
}


Instruction* Gen::VisitAssign(AstNode* stmt) {
  Instruction* rhs = Visit(stmt->rhs());

  if (stmt->lhs()->is(AstNode::kValue)) {
    AstValue* value = AstValue::Cast(stmt->lhs());

    if (value->slot()->is_stack()) {
      // No instruction is needed
      Assign(value->slot(), rhs);
    } else {
      Add(kStoreContext, value->slot())->AddArg(rhs);
    }
  } else if (stmt->lhs()->is(AstNode::kMember)) {
    Instruction* property = Visit(stmt->lhs()->rhs());
    Instruction* receiver = Visit(stmt->lhs()->lhs());

    Add(kStoreProperty)->AddArg(receiver)->AddArg(property)->AddArg(rhs);
  } else {
    // TODO: Set error! Incorrect lhs
    abort();
  }

  return rhs;
}


Instruction* Gen::VisitReturn(AstNode* stmt) {
  return Return(kReturn)->AddArg(Visit(stmt->lhs()));
}


Instruction* Gen::VisitValue(AstNode* stmt) {
  AstValue* value = AstValue::Cast(stmt);
  ScopeSlot* slot = value->slot();
  if (slot->is_stack()) {
    if (slot->hir() != NULL && slot->hir()->block() == current_block()) {
      // Local value
      return slot->hir();
    } else {
      // External value
      return Add(kPhi, slot);
    }
  } else {
    return Add(kLoadContext, slot);
  }
}


Instruction* Gen::VisitIf(AstNode* stmt) {
  Block* t = CreateBlock();
  Block* f = CreateBlock();
  Instruction* cond = Visit(stmt->lhs());

  Branch(kIf, t, f)->AddArg(cond);

  set_current_block(t);
  Visit(stmt->rhs());
  t = current_block();

  AstList::Item* else_branch = stmt->children()->head()->next()->next();

  if (else_branch != NULL) {
    set_current_block(f);
    Visit(else_branch->value());
    f = current_block();
  }

  set_current_block(Join(t, f));
}


// Literals


Instruction* Gen::VisitLiteral(AstNode* stmt) {
  return Add(kLiteral, root_.Put(stmt));
}


Instruction* Gen::VisitNumber(AstNode* stmt) {
  return VisitLiteral(stmt);
}


Instruction* Gen::VisitNil(AstNode* stmt) {
  return VisitLiteral(stmt);
}


Instruction* Gen::VisitTrue(AstNode* stmt) {
  return VisitLiteral(stmt);
}


Instruction* Gen::VisitFalse(AstNode* stmt) {
  return VisitLiteral(stmt);
}


Instruction* Gen::VisitString(AstNode* stmt) {
  return VisitLiteral(stmt);
}


Instruction* Gen::VisitProperty(AstNode* stmt) {
  return VisitLiteral(stmt);
}


Block::Block(Gen* g) : id(g->block_id()),
                       g_(g),
                       loop_(false),
                       ended_(false),
                       pred_count_(0),
                       succ_count_(0) {
  pred_[0] = NULL;
  pred_[1] = NULL;
  succ_[0] = NULL;
  succ_[1] = NULL;
}


void Block::AddPredecessor(Block* b) {
  assert(pred_count_ < 2);
  pred_[pred_count_++] = b;

  // Propagate values from parent block and create phis if needed

}

} // namespace hir
} // namespace internal
} // namespace candor
