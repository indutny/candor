#include "hir.h"
#include "hir-inl.h"
#include <string.h> // memset, memcpy

#define __STDC_FORMAT_MACROS
#include <inttypes.h> // PRIu64

namespace candor {
namespace internal {

HIRGen::HIRGen(Heap* heap, AstNode* root) : Visitor<HIRInstruction>(kPreorder),
                                            current_block_(NULL),
                                            current_root_(NULL),
                                            break_continue_info_(NULL),
                                            root_(heap),
                                            block_id_(0),
                                            instr_id_(-2) {
  work_queue_.Push(new HIRFunction(this, NULL, root));

  while (work_queue_.length() != 0) {
    HIRFunction* current = HIRFunction::Cast(work_queue_.Shift());

    HIRBlock* b = CreateBlock(current->ast()->stack_slots());
    set_current_block(b);
    set_current_root(b);

    roots_.Push(b);

    current->body = b;
    Visit(current->ast());
  }

  PruneHIRPhis();
}


void HIRGen::PruneHIRPhis() {
  HIRBlockList::Item* head = blocks_.head();
  for (; head != NULL; head = head->next()) {
    head->value()->PruneHIRPhis();
  }
}


HIRInstruction* HIRGen::VisitFunction(AstNode* stmt) {
  FunctionLiteral* fn = FunctionLiteral::Cast(stmt);

  if (current_root() == current_block() &&
      current_block()->IsEmpty()) {
    Add(HIRInstruction::kEntry);

    AstList::Item* args_head = fn->args()->head();
    for (int i = 0; args_head != NULL; args_head = args_head->next(), i++) {
      AstValue* value = AstValue::Cast(args_head->value());

      HIRInstruction* arg = Add(new HIRLoadArg(this, current_block(), i));
      if (value->slot()->is_stack()) {
        // No instruction is needed
        Assign(value->slot(), arg);
      } else {
        Add(HIRInstruction::kStoreContext, value->slot())->AddArg(arg);
      }
    }

    VisitChildren(stmt);

    if (!current_block()->IsEnded()) {
      HIRInstruction* val = Add(HIRInstruction::kNil);
      HIRInstruction* end = Return(HIRInstruction::kReturn);
      end->AddArg(val);
    }

    return NULL;
  } else {
    HIRFunction* f = new HIRFunction(this, current_block(), stmt);
    work_queue_.Push(f);
    return Add(f);
  }
}


HIRInstruction* HIRGen::VisitAssign(AstNode* stmt) {
  HIRInstruction* rhs = Visit(stmt->rhs());

  if (stmt->lhs()->is(AstNode::kValue)) {
    AstValue* value = AstValue::Cast(stmt->lhs());

    if (value->slot()->is_stack()) {
      // No instruction is needed
      Assign(value->slot(), rhs);
    } else {
      Add(HIRInstruction::kStoreContext, value->slot())->AddArg(rhs);
    }
  } else if (stmt->lhs()->is(AstNode::kMember)) {
    HIRInstruction* property = Visit(stmt->lhs()->rhs());
    HIRInstruction* receiver = Visit(stmt->lhs()->lhs());

    Add(HIRInstruction::kStoreProperty)
        ->AddArg(receiver)
        ->AddArg(property)
        ->AddArg(rhs);
  } else {
    // TODO: Set error! Incorrect lhs
    abort();
  }

  return rhs;
}


HIRInstruction* HIRGen::VisitReturn(AstNode* stmt) {
  return Return(HIRInstruction::kReturn)->AddArg(Visit(stmt->lhs()));
}


HIRInstruction* HIRGen::VisitValue(AstNode* stmt) {
  AstValue* value = AstValue::Cast(stmt);
  ScopeSlot* slot = value->slot();
  if (slot->is_stack()) {
    HIRInstruction* i = current_block()->env()->At(slot);

    if (i != NULL && i->block() == current_block()) {
      // Local value
      return i;
    } else {
      HIRPhi* phi = CreatePhi(slot);
      if (i != NULL) phi->AddInput(i);

      // External value
      return Add(Assign(slot, phi));
    }
  } else {
    return Add(HIRInstruction::kLoadContext, slot);
  }
}


HIRInstruction* HIRGen::VisitIf(AstNode* stmt) {
  HIRBlock* t = CreateBlock();
  HIRBlock* f = CreateBlock();
  HIRInstruction* cond = Visit(stmt->lhs());

  Branch(HIRInstruction::kIf, t, f)->AddArg(cond);

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


HIRInstruction* HIRGen::VisitWhile(AstNode* stmt) {
  BreakContinueInfo* old = break_continue_info_;
  HIRBlock* start = CreateBlock();

  Goto(HIRInstruction::kGoto, start);

  // HIRBlock can't be join and branch at the same time
  set_current_block(CreateBlock());
  start->MarkLoop();
  start->Goto(HIRInstruction::kGoto, current_block());

  HIRInstruction* cond = Visit(stmt->lhs());

  HIRBlock* body = CreateBlock();
  HIRBlock* loop = CreateBlock();
  HIRBlock* end = CreateBlock();

  Branch(HIRInstruction::kIf, body, end)->AddArg(cond);

  set_current_block(body);
  break_continue_info_ = new BreakContinueInfo(this, end);

  Visit(stmt->rhs());

  while (break_continue_info_->continue_blocks()->length() > 0) {
    HIRBlock* next = break_continue_info_->continue_blocks()->Shift();
    Goto(HIRInstruction::kGoto, next);
    set_current_block(next);
  }
  Goto(HIRInstruction::kGoto, loop);
  loop->Goto(HIRInstruction::kGoto, start);

  // Next current block should not be a join
  set_current_block(break_continue_info_->GetBreak());

  // Restore break continue info
  break_continue_info_ = old;
}


HIRInstruction* HIRGen::VisitBreak(AstNode* stmt) {
  assert(break_continue_info_ != NULL);
  Goto(HIRInstruction::kGoto, break_continue_info_->GetBreak());
}


HIRInstruction* HIRGen::VisitContinue(AstNode* stmt) {
  assert(break_continue_info_ != NULL);
  Goto(HIRInstruction::kGoto, break_continue_info_->GetContinue());
}


HIRInstruction* HIRGen::VisitUnOp(AstNode* stmt) {
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
      HIRInstruction* ione = Visit(one);
      HIRInstruction* res = Visit(op->lhs());
      HIRInstruction* bin = Add(HIRInstruction::kBinOp)->AddArg(ione)
                                                       ->AddArg(res);

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
    return Add(HIRInstruction::kNot)->AddArg(Visit(op->lhs()));
  } else {
    UNEXPECTED
  }
}


HIRInstruction* HIRGen::VisitBinOp(AstNode* stmt) {
  BinOp* op = BinOp::Cast(stmt);
  HIRInstruction* res;

  if (!BinOp::is_bool_logic(op->subtype())) {
    res = Add(HIRInstruction::kBinOp)
        ->AddArg(Visit(op->lhs()))
        ->AddArg(Visit(op->rhs()));
  } else {
    HIRInstruction* lhs = Visit(op->lhs());
    HIRBlock* branch = CreateBlock();
    ScopeSlot* slot = current_block()->env()->logic_slot();

    Goto(HIRInstruction::kGoto, branch);
    set_current_block(branch);

    HIRBlock* t = CreateBlock();
    HIRBlock* f = CreateBlock();

    Branch(HIRInstruction::kIf, t, f)->AddArg(lhs);

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
    HIRPhi* phi =  current_block()->env()->PhiAt(slot);
    assert(phi != NULL);

    return phi;
  }

  res->ast(stmt);
  return res;
}


HIRInstruction* HIRGen::VisitObjectLiteral(AstNode* stmt) {
  HIRInstruction* res = Add(HIRInstruction::kAllocateObject);
  ObjectLiteral* obj = ObjectLiteral::Cast(stmt);

  AstList::Item* khead = obj->keys()->head();
  AstList::Item* vhead = obj->values()->head();
  for (; khead != NULL; khead = khead->next(), vhead = vhead->next()) {
    HIRInstruction* value = Visit(vhead->value());
    HIRInstruction* key = Visit(khead->value());

    Add(HIRInstruction::kStoreProperty)
        ->AddArg(res)
        ->AddArg(key)
        ->AddArg(value);
  }

  return res;
}


HIRInstruction* HIRGen::VisitArrayLiteral(AstNode* stmt) {
  HIRInstruction* res = Add(HIRInstruction::kAllocateArray);

  AstList::Item* head = stmt->children()->head();
  for (uint64_t i = 0; head != NULL; head = head->next(), i++) {
    AstNode* index = new AstNode(AstNode::kNumber);
    char keystr[32];
    index->value(keystr);
    index->length(snprintf(keystr, sizeof(keystr), "%" PRIu64, i));

    HIRInstruction* key = Visit(index);
    HIRInstruction* value = Visit(head->value());

    // keystr is on-stack variable, nullify ast just for sanity
    key->ast(NULL);

    Add(HIRInstruction::kStoreProperty)
        ->AddArg(res)
        ->AddArg(key)
        ->AddArg(value);
  }

  return res;
}


HIRInstruction* HIRGen::VisitMember(AstNode* stmt) {
  HIRInstruction* prop = Visit(stmt->rhs());
  HIRInstruction* recv = Visit(stmt->lhs());
  return Add(HIRInstruction::kLoadProperty)->AddArg(recv)->AddArg(prop);
}


HIRInstruction* HIRGen::VisitDelete(AstNode* stmt) {
  HIRInstruction* prop = Visit(stmt->lhs()->rhs());
  HIRInstruction* recv = Visit(stmt->lhs()->lhs());
  Add(HIRInstruction::kDeleteProperty)->AddArg(recv)->AddArg(prop);

  // Delete property returns nil
  return Add(HIRInstruction::kNil);
}


HIRInstruction* HIRGen::VisitCall(AstNode* stmt) {
  FunctionLiteral* fn = FunctionLiteral::Cast(stmt);

  // handle __$gc() and __$trace() calls
  if (fn->variable()->is(AstNode::kValue)) {
    AstNode* name = AstValue::Cast(fn->variable())->name();
    if (name->length() == 5 && strncmp(name->value(), "__$gc", 5) == 0) {
      return Add(HIRInstruction::kCollectGarbage);
    } else if (name->length() == 8 &&
               strncmp(name->value(), "__$trace", 8) == 0) {
      return Add(HIRInstruction::kGetStackTrace);
    }
  }

  HIRInstruction* receiver = NULL;

  if (fn->args()->length() > 0 &&
      fn->args()->head()->value()->is(AstNode::kSelf)) {
    receiver = Visit(fn->variable()->lhs());
  }

  HIRInstructionList args;

  AstList::Item* item = fn->args()->head();
  for (; item != NULL; item = item->next()) {
    if (item->value()->is(AstNode::kSelf)) {
      assert(receiver != NULL);
      args.Push(receiver);
    } else {
      args.Push(Visit(item->value()));
    }
  }

  HIRInstruction* var;
  if (fn->args()->length() > 0 &&
      fn->args()->head()->value()->is(AstNode::kSelf)) {
    assert(fn->variable()->is(AstNode::kMember));

    HIRInstruction* property = Visit(fn->variable()->rhs());

    var = Add(HIRInstruction::kLoadProperty)->AddArg(receiver)
                                            ->AddArg(property);
  } else {
    var = Visit(fn->variable());
  }
  HIRInstruction* call = CreateInstruction(HIRInstruction::kCall)->AddArg(var);

  HIRInstructionList::Item* ihead = args.head();
  for (; ihead != NULL; ihead = ihead->next()) {
    call->AddArg(ihead->value());
  }

  return Add(call);
}


HIRInstruction* HIRGen::VisitTypeof(AstNode* stmt) {
  return Add(HIRInstruction::kTypeof)->AddArg(Visit(stmt->lhs()));
}


HIRInstruction* HIRGen::VisitKeysof(AstNode* stmt) {
  return Add(HIRInstruction::kKeysof)->AddArg(Visit(stmt->lhs()));
}

HIRInstruction* HIRGen::VisitSizeof(AstNode* stmt) {
  return Add(HIRInstruction::kSizeof)->AddArg(Visit(stmt->lhs()));
}


HIRInstruction* HIRGen::VisitClone(AstNode* stmt) {
  return Add(HIRInstruction::kClone)->AddArg(Visit(stmt->lhs()));
}


// Literals


HIRInstruction* HIRGen::VisitLiteral(AstNode* stmt) {
  HIRInstruction* i = Add(HIRInstruction::kLiteral, root_.Put(stmt));

  i->ast(stmt);

  return i;
}


HIRInstruction* HIRGen::VisitNumber(AstNode* stmt) {
  return VisitLiteral(stmt);
}


HIRInstruction* HIRGen::VisitNil(AstNode* stmt) {
  return Add(HIRInstruction::kNil);
}


HIRInstruction* HIRGen::VisitTrue(AstNode* stmt) {
  return VisitLiteral(stmt);
}


HIRInstruction* HIRGen::VisitFalse(AstNode* stmt) {
  return VisitLiteral(stmt);
}


HIRInstruction* HIRGen::VisitString(AstNode* stmt) {
  return VisitLiteral(stmt);
}


HIRInstruction* HIRGen::VisitProperty(AstNode* stmt) {
  return VisitLiteral(stmt);
}


HIRBlock::HIRBlock(HIRGen* g) : id(g->block_id()),
                          g_(g),
                          loop_(false),
                          ended_(false),
                          env_(NULL),
                          pred_count_(0),
                          succ_count_(0),
                          start_id_(-1),
                          end_id_(-1) {
  pred_[0] = NULL;
  pred_[1] = NULL;
  succ_[0] = NULL;
  succ_[1] = NULL;
}


HIRInstruction* HIRBlock::Assign(ScopeSlot* slot, HIRInstruction* value) {
  assert(value != NULL);

  value->slot(slot);
  env()->Set(slot, value);

  return value;
}


void HIRBlock::AddPredecessor(HIRBlock* b) {
  assert(pred_count_ < 2);
  pred_[pred_count_++] = b;

  if (pred_count_ == 1) {
    // Fast path - copy environment
    env()->Copy(b->env());
    return;
  }

  for (int i = 0; i < b->env()->stack_slots(); i++) {
    HIRInstruction* curr = b->env()->At(i);
    if (curr == NULL) continue;

    HIRInstruction* old = this->env()->At(i);

    // Value already present in block
    if (old != NULL) {
      HIRPhi* phi = this->env()->PhiAt(i);

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


void HIRBlock::MarkLoop() {
  loop_ = true;

  // Create phi for every possible value (except logic_slot)
  for (int i = 0; i < env()->stack_slots() - 1; i++) {
    ScopeSlot* slot = new ScopeSlot(ScopeSlot::kStack);
    slot->index(i);

    HIRInstruction* old = env()->At(i);
    HIRPhi* phi = CreatePhi(slot);
    if (old != NULL) phi->AddInput(old);

    Add(Assign(slot, phi));
  }
}


void HIRGen::Replace(HIRInstruction* o, HIRInstruction* n) {
  HIRInstructionList::Item* head = o->uses()->head();
  for (; head != NULL; head = head->next()) {
    HIRInstruction* use = head->value();

    use->ReplaceArg(o, n);
  }
}


void HIRBlock::Remove(HIRInstruction* instr) {
  HIRInstructionList::Item* head = instructions_.head();
  for (; head != NULL; head = head->next()) {
    HIRInstruction* i = head->value();

    if (i == instr) {
      instructions_.Remove(head);
      break;
    }
  }

  instr->Remove();
}


void HIRBlock::PruneHIRPhis() {
  HIRPhiList queue_;

  HIRPhiList::Item* head = phis_.head();
  for (; head != NULL; head = head->next()) {
    queue_.Push(head->value());
  }

  while (queue_.length() > 0) {
    HIRPhi* phi = queue_.Shift();

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
      if (phi->InputAt(i)->Is(HIRInstruction::kPhi)) {
        queue_.Push(HIRPhi::Cast(phi->InputAt(i)));
      }
    }
  }
}


HIREnvironment::HIREnvironment(int stack_slots)
    : stack_slots_(stack_slots + 1) {
  // ^^ NOTE: One stack slot is reserved for bool logic binary operations
  logic_slot_ = new ScopeSlot(ScopeSlot::kStack);
  logic_slot_->index(stack_slots);

  instructions_ = reinterpret_cast<HIRInstruction**>(Zone::current()->Allocate(
      sizeof(*instructions_) * stack_slots_));
  memset(instructions_, 0, sizeof(*instructions_) * stack_slots_);

  phis_ = reinterpret_cast<HIRPhi**>(Zone::current()->Allocate(
      sizeof(*phis_) * stack_slots_));
  memset(phis_, 0, sizeof(*phis_) * stack_slots_);
}


void HIREnvironment::Copy(HIREnvironment* from) {
  memcpy(instructions_,
         from->instructions_,
         sizeof(*instructions_) * stack_slots_);
  memcpy(phis_,
         from->phis_,
         sizeof(*phis_) * stack_slots_);
}


BreakContinueInfo::BreakContinueInfo(HIRGen *g, HIRBlock* end) : g_(g),
                                                                 brk_(end) {
}


HIRBlock* BreakContinueInfo::GetContinue() {
  HIRBlock* b = g_->CreateBlock();

  continue_blocks()->Push(b);

  return b;
}


HIRBlock* BreakContinueInfo::GetBreak() {
  HIRBlock* b = g_->CreateBlock();
  brk_->Goto(HIRInstruction::kGoto, b);
  brk_ = b;

  return b;
}

} // namespace internal
} // namespace candor
