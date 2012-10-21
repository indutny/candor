#include "hir.h"
#include "hir-inl.h"
#include <string.h> // memset, memcpy

namespace candor {
namespace internal {
namespace hir {

Gen::Gen(Heap* heap, AstNode* root) : Visitor<Instruction>(kPreorder),
                                      current_block_(NULL),
                                      current_root_(NULL),
                                      root_(heap),
                                      block_id_(0),
                                      // First instruction doesn't appear in HIR
                                      instr_id_(-2) {
  work_queue_.Push(new Function(this, NULL, root));

  while (work_queue_.length() != 0) {
    Function* current = Function::Cast(work_queue_.Shift());

    Block* b = CreateBlock(current->ast()->stack_slots());
    set_current_block(b);
    set_current_root(b);

    current->body = b;
    Visit(current->ast());
  }

  PrunePhis();
}


void Gen::PrunePhis() {
  BlockList::Item* head = blocks_.head();
  for (; head != NULL; head = head->next()) {
    head->value()->PrunePhis();
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
    Instruction* i = current_block()->env()->At(slot);

    if (i != NULL && i->block() == current_block()) {
      // Local value
      return i;
    } else {
      Phi* phi = CreatePhi(slot);
      if (i != NULL) phi->AddInput(i);

      // External value
      return Add(Assign(slot, phi));
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
                       env_(NULL),
                       pred_count_(0),
                       succ_count_(0) {
  pred_[0] = NULL;
  pred_[1] = NULL;
  succ_[0] = NULL;
  succ_[1] = NULL;
}


Instruction* Block::Assign(ScopeSlot* slot, Instruction* value) {
  value->slot(slot);
  env()->Set(slot, value);

  return value;
}


void Block::AddPredecessor(Block* b) {
  assert(pred_count_ < 2);
  pred_[pred_count_++] = b;

  if (pred_count_ == 1) {
    // Fast path - copy environment
    env()->Copy(b->env());
    return;
  }

  for (int i = 0; i < b->env()->stack_slots(); i++) {
    Instruction* curr = b->env()->At(i);
    Instruction* old = this->env()->At(i);

    // Value already present in block
    if (old != NULL) {
      Phi* phi = this->env()->PhiAt(i);

      // In loop values can be propagated up to the block where it was declared
      if (old == curr) continue;

      // Create phi if needed
      if (phi == NULL || phi->block() != this) {
        assert(IsEmpty());
        phi = CreatePhi(curr->slot());
        Add(phi);
        phi->AddInput(old);

        Assign(curr->slot(), phi);
      }

      // Add value as phi's input
      phi->AddInput(curr);
    } else {
      // Propagate value
      this->env()->Set(i, curr);
    }
  }
}


void Block::Replace(Instruction* o, Instruction* n) {
  InstructionList::Item* head = o->uses()->head();
  for (; head != NULL; head = head->next()) {
    Instruction* use = head->value();

    if (use->block() == this) {
      use->ReplaceArg(o, n);
    }
  }
}


void Block::Remove(Instruction* instr) {
}


void Block::PrunePhis() {
  PhiList::Item* head = phis_.head();
  for (; head != NULL; head = head->next()) {
    Phi* phi = head->value();

    switch (phi->input_count()) {
     case 0:
      phi->Nilify();
      break;
     case 1:
      Replace(phi, phi->InputAt(0));
      Remove(phi);
      break;
     case 2:
      // Good phi, nothing to do here
      break;
     default:
      UNEXPECTED
    }
  }
}


Environment::Environment(int stack_slots) : stack_slots_(stack_slots) {
  instructions_ = reinterpret_cast<Instruction**>(Zone::current()->Allocate(
      sizeof(*instructions_) * stack_slots_));
  memset(instructions_, 0, sizeof(*instructions_) * stack_slots_);

  phis_ = reinterpret_cast<Phi**>(Zone::current()->Allocate(
      sizeof(*phis_) * stack_slots_));
  memset(phis_, 0, sizeof(*phis_) * stack_slots_);
}


void Environment::Copy(Environment* from) {
  memcpy(instructions_,
         from->instructions_,
         sizeof(*instructions_) * stack_slots_);
  memcpy(phis_,
         from->phis_,
         sizeof(*phis_) * stack_slots_);
}

} // namespace hir
} // namespace internal
} // namespace candor
