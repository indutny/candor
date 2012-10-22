#include "hir.h"
#include "hir-inl.h"
#include <string.h> // memset, memcpy

#define __STDC_FORMAT_MACROS
#include <inttypes.h> // PRIu64

namespace candor {
namespace internal {
namespace hir {

Gen::Gen(Heap* heap, AstNode* root) : Visitor<Instruction>(kPreorder),
                                      current_block_(NULL),
                                      current_root_(NULL),
                                      break_continue_info_(NULL),
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
    Instruction* entry = CreateInstruction(Instruction::kEntry);

    AstList::Item* args_head = fn->args()->head();
    for (; args_head != NULL; args_head = args_head->next()) {
      entry->AddArg(Visit(args_head->value()));
    }

    Add(entry);

    VisitChildren(stmt);

    if (!current_block()->IsEnded()) {
      Instruction* val = Add(Instruction::kNil);
      Instruction* end = Return(Instruction::kReturn);
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
      Add(Instruction::kStoreContext, value->slot())->AddArg(rhs);
    }
  } else if (stmt->lhs()->is(AstNode::kMember)) {
    Instruction* property = Visit(stmt->lhs()->rhs());
    Instruction* receiver = Visit(stmt->lhs()->lhs());

    Add(Instruction::kStoreProperty)
        ->AddArg(receiver)
        ->AddArg(property)
        ->AddArg(rhs);
  } else {
    // TODO: Set error! Incorrect lhs
    abort();
  }

  return rhs;
}


Instruction* Gen::VisitReturn(AstNode* stmt) {
  return Return(Instruction::kReturn)->AddArg(Visit(stmt->lhs()));
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
    return Add(Instruction::kLoadContext, slot);
  }
}


Instruction* Gen::VisitIf(AstNode* stmt) {
  Block* t = CreateBlock();
  Block* f = CreateBlock();
  Instruction* cond = Visit(stmt->lhs());

  Branch(Instruction::kIf, t, f)->AddArg(cond);

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


Instruction* Gen::VisitWhile(AstNode* stmt) {
  BreakContinueInfo* old = break_continue_info_;
  Block* start = CreateBlock();

  Goto(Instruction::kGoto, start);

  // Block can't be join and branch at the same time
  set_current_block(CreateBlock());
  start->MarkLoop();
  start->Goto(Instruction::kGoto, current_block());

  Instruction* cond = Visit(stmt->lhs());

  Block* body = CreateBlock();
  Block* loop = CreateBlock();
  Block* end = CreateBlock();

  Branch(Instruction::kIf, body, end)->AddArg(cond);

  set_current_block(body);
  break_continue_info_ = new BreakContinueInfo(this, end);

  Visit(stmt->rhs());

  while (break_continue_info_->continue_blocks()->length() > 0) {
    Block* next = break_continue_info_->continue_blocks()->Shift();
    Goto(Instruction::kGoto, next);
    set_current_block(next);
  }
  Goto(Instruction::kGoto, loop);
  loop->Goto(Instruction::kGoto, start);

  // Next current block should not be a join
  set_current_block(break_continue_info_->GetBreak());

  // Restore break continue info
  break_continue_info_ = old;
}


Instruction* Gen::VisitBreak(AstNode* stmt) {
  assert(break_continue_info_ != NULL);
  Goto(Instruction::kGoto, break_continue_info_->GetBreak());
}


Instruction* Gen::VisitContinue(AstNode* stmt) {
  assert(break_continue_info_ != NULL);
  Goto(Instruction::kGoto, break_continue_info_->GetContinue());
}


Instruction* Gen::VisitUnOp(AstNode* stmt) {
  UnOp* op = UnOp::Cast(stmt);

  if (op->is_changing()) {
    // ++i, i++
    AstNode* one = new AstNode(AstNode::kNumber, stmt);
    ScopeSlot* slot = AstValue::Cast(op->lhs())->slot();

    one->value("1");
    one->length(1);

    AstNode* wrap = new BinOp(
        (op->subtype() == UnOp::kPreInc || op->subtype() == UnOp::kPostInc) ?
            BinOp::kAdd : BinOp::kSub,
        one,
        op->lhs());

    if (op->subtype() == UnOp::kPreInc || op->subtype() == UnOp::kPreDec) {
      return Assign(slot, Visit(wrap));
    } else {
      Instruction* ione = Visit(one);
      Instruction* res = Visit(op->lhs());
      Instruction* bin = Add(Instruction::kBinOp)->AddArg(ione)->AddArg(res);

      bin->ast(wrap);
      Assign(slot, bin);

      return res;
    }
  } else if (op->subtype() == UnOp::kPlus || op->subtype() == UnOp::kMinus) {
    // +i = 0 + i,
    // -i = 0 - i
    AstNode* zero = new AstNode(AstNode::kNumber, stmt);
    zero->value("0");
    zero->length(1);

    AstNode* wrap = new BinOp(
        op->subtype() == UnOp::kPlus ? BinOp::kAdd : BinOp::kSub,
        zero,
        op->lhs());

    return Visit(wrap);
  } else if (op->subtype() == UnOp::kNot) {
    return Add(Instruction::kNot)->AddArg(Visit(op->lhs()));
  } else {
    UNEXPECTED
  }
}


Instruction* Gen::VisitBinOp(AstNode* stmt) {
  BinOp* op = BinOp::Cast(stmt);
  Instruction* res;

  if (!BinOp::is_bool_logic(op->subtype())) {
    res = Add(Instruction::kBinOp)
        ->AddArg(Visit(op->lhs()))
        ->AddArg(Visit(op->rhs()));
  } else {
    Instruction* lhs = Visit(op->lhs());
    Block* branch = CreateBlock();
    ScopeSlot* slot = current_block()->env()->logic_slot();

    Goto(Instruction::kGoto, branch);
    set_current_block(branch);

    Block* t = CreateBlock();
    Block* f = CreateBlock();

    Branch(Instruction::kIf, t, f)->AddArg(lhs);

    set_current_block(t);
    if (op->subtype() == BinOp::kLAnd) {
      Assign(slot, Visit(op->rhs()));
    } else {
      Assign(slot, lhs);
    }
    t = current_block();

    set_current_block(f);
    if (op->subtype() == BinOp::kLAnd) {
      Assign(slot, lhs);
    } else {
      Assign(slot, Visit(op->rhs()));
    }
    f = current_block();

    set_current_block(Join(t, f));
    Phi* phi =  current_block()->env()->PhiAt(slot);
    assert(phi != NULL);

    return phi;
  }

  res->ast(stmt);
  return res;
}


Instruction* Gen::VisitObjectLiteral(AstNode* stmt) {
  Instruction* res = Add(Instruction::kAllocateObject);
  ObjectLiteral* obj = ObjectLiteral::Cast(stmt);

  AstList::Item* khead = obj->keys()->head();
  AstList::Item* vhead = obj->values()->head();
  for (; khead != NULL; khead = khead->next(), vhead = vhead->next()) {
    Instruction* value = Visit(vhead->value());
    Instruction* key = Visit(khead->value());

    Add(Instruction::kStoreProperty)->AddArg(res)->AddArg(key)->AddArg(value);
  }

  return res;
}


Instruction* Gen::VisitArrayLiteral(AstNode* stmt) {
  Instruction* res = Add(Instruction::kAllocateArray);

  AstList::Item* head = stmt->children()->head();
  for (uint64_t i = 0; head != NULL; head = head->next(), i++) {
    AstNode* index = new AstNode(AstNode::kNumber);
    char keystr[32];
    index->value(keystr);
    index->length(snprintf(keystr, sizeof(keystr), "%" PRIu64, i));

    Instruction* key = Visit(index);
    Instruction* value = Visit(head->value());

    // keystr is on-stack variable, nullify ast just for sanity
    key->ast(NULL);

    Add(Instruction::kStoreProperty)->AddArg(res)->AddArg(key)->AddArg(value);
  }

  return res;
}


Instruction* Gen::VisitMember(AstNode* stmt) {
  Instruction* prop = Visit(stmt->rhs());
  Instruction* recv = Visit(stmt->lhs());
  return Add(Instruction::kLoadProperty)->AddArg(recv)->AddArg(prop);
}


Instruction* Gen::VisitDelete(AstNode* stmt) {
  Instruction* prop = Visit(stmt->lhs()->rhs());
  Instruction* recv = Visit(stmt->lhs()->lhs());
  return Add(Instruction::kDeleteProperty)->AddArg(recv)->AddArg(prop);
}


Instruction* Gen::VisitCall(AstNode* stmt) {
  FunctionLiteral* fn = FunctionLiteral::Cast(stmt);

  // handle __$gc() and __$trace() calls
  if (fn->variable()->is(AstNode::kValue)) {
    AstNode* name = AstValue::Cast(fn->variable())->name();
    if (name->length() == 5 && strncmp(name->value(), "__$gc", 5) == 0) {
      return Add(Instruction::kCollectGarbage);
    } else if (name->length() == 8 &&
               strncmp(name->value(), "__$trace", 8) == 0) {
      return Add(Instruction::kGetStackTrace);
    }
  }

  Instruction* receiver = NULL;

  if (fn->args()->length() > 0 &&
      fn->args()->head()->value()->is(AstNode::kSelf)) {
    receiver = Visit(fn->variable()->lhs());
  }

  InstructionList args;

  AstList::Item* item = fn->args()->head();
  for (; item != NULL; item = item->next()) {
    if (item->value()->is(AstNode::kSelf)) {
      assert(receiver != NULL);
      args.Push(receiver);
    } else {
      args.Push(Visit(item->value()));
    }
  }

  Instruction* var;
  if (fn->args()->length() > 0 &&
      fn->args()->head()->value()->is(AstNode::kSelf)) {
    assert(fn->variable()->is(AstNode::kMember));

    Instruction* property = Visit(fn->variable()->rhs());

    var = Add(Instruction::kLoadProperty)->AddArg(receiver)->AddArg(property);
  } else {
    var = Visit(fn->variable());
  }
  Instruction* call = CreateInstruction(Instruction::kCall)->AddArg(var);

  InstructionList::Item* ihead = args.head();
  for (; ihead != NULL; ihead = ihead->next()) {
    call->AddArg(ihead->value());
  }

  return Add(call);
}


Instruction* Gen::VisitTypeof(AstNode* stmt) {
  return Add(Instruction::kTypeof)->AddArg(Visit(stmt->lhs()));
}


Instruction* Gen::VisitKeysof(AstNode* stmt) {
  return Add(Instruction::kKeysof)->AddArg(Visit(stmt->lhs()));
}

Instruction* Gen::VisitSizeof(AstNode* stmt) {
  return Add(Instruction::kSizeof)->AddArg(Visit(stmt->lhs()));
}


// Literals


Instruction* Gen::VisitLiteral(AstNode* stmt) {
  Instruction* i = Add(Instruction::kLiteral, root_.Put(stmt));

  i->ast(stmt);

  return i;
}


Instruction* Gen::VisitNumber(AstNode* stmt) {
  return VisitLiteral(stmt);
}


Instruction* Gen::VisitNil(AstNode* stmt) {
  return Add(Instruction::kNil);
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
  assert(value != NULL);

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
    if (curr == NULL) continue;

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


void Block::MarkLoop() {
  loop_ = true;

  // Create phi for every possible value (except logic_slot)
  for (int i = 0; i < env()->stack_slots() - 1; i++) {
    ScopeSlot* slot = new ScopeSlot(ScopeSlot::kStack);
    slot->index(i);

    Instruction* old = env()->At(i);
    Phi* phi = CreatePhi(slot);
    if (old != NULL) phi->AddInput(old);

    Add(Assign(slot, phi));
  }
}


void Gen::Replace(Instruction* o, Instruction* n) {
  InstructionList::Item* head = o->uses()->head();
  for (; head != NULL; head = head->next()) {
    Instruction* use = head->value();

    use->ReplaceArg(o, n);
  }
}


void Block::Remove(Instruction* instr) {
  InstructionList::Item* head = instructions_.head();
  for (; head != NULL; head = head->next()) {
    Instruction* i = head->value();

    if (i == instr) {
      instructions_.Remove(head);
      break;
    }
  }

  instr->Remove();
}


void Block::PrunePhis() {
  PhiList queue_;

  PhiList::Item* head = phis_.head();
  for (; head != NULL; head = head->next()) {
    queue_.Push(head->value());
  }

  while (queue_.length() > 0) {
    Phi* phi = queue_.Shift();

    switch (phi->input_count()) {
     case 0:
      phi->Nilify();
      break;
     case 1:
      g_->Replace(phi, phi->InputAt(0));
      if (phi->block() == this) Remove(phi);
      break;
     case 2:
      break;
     default:
      UNEXPECTED
    }

    // Check recursive uses too
    for (int i = 0; i < phi->input_count(); i++) {
      if (phi->InputAt(i)->Is(Instruction::kPhi)) {
        queue_.Push(Phi::Cast(phi->InputAt(i)));
      }
    }
  }
}


Environment::Environment(int stack_slots) : stack_slots_(stack_slots + 1) {
  // ^^ NOTE: One stack slot is reserved for bool logic binary operations
  logic_slot_ = new ScopeSlot(ScopeSlot::kStack);
  logic_slot_->index(stack_slots);

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


BreakContinueInfo::BreakContinueInfo(Gen *g, Block* end) : g_(g), brk_(end) {
}


Block* BreakContinueInfo::GetContinue() {
  Block* b = g_->CreateBlock();

  continue_blocks()->Push(b);

  return b;
}


Block* BreakContinueInfo::GetBreak() {
  Block* b = g_->CreateBlock();
  brk_->Goto(Instruction::kGoto, b);
  brk_ = b;

  return b;
}

} // namespace hir
} // namespace internal
} // namespace candor
