#include "hir.h"
#include "hir-inl.h"
#include <string.h> // memset, memcpy

#define __STDC_FORMAT_MACROS
#include <inttypes.h> // PRIu64

namespace candor {
namespace internal {
namespace hir {

HGen::HGen(Heap* heap, AstNode* root) : Visitor<HInstruction>(kPreorder),
                                        current_block_(NULL),
                                        current_root_(NULL),
                                        break_continue_info_(NULL),
                                        root_(heap),
                                        block_id_(0),
                                        // First instruction doesn't appear in
                                        // HIR
                                        instr_id_(-2) {
  work_queue_.Push(new HFunction(this, NULL, root));

  while (work_queue_.length() != 0) {
    HFunction* current = HFunction::Cast(work_queue_.Shift());

    HBlock* b = CreateBlock(current->ast()->stack_slots());
    set_current_block(b);
    set_current_root(b);

    current->body = b;
    Visit(current->ast());
  }

  PruneHPhis();
}


void HGen::PruneHPhis() {
  HBlockList::Item* head = blocks_.head();
  for (; head != NULL; head = head->next()) {
    head->value()->PruneHPhis();
  }
}


HInstruction* HGen::VisitFunction(AstNode* stmt) {
  FunctionLiteral* fn = FunctionLiteral::Cast(stmt);

  if (current_root() == current_block() &&
      current_block()->IsEmpty()) {
    HInstruction* entry = CreateInstruction(HInstruction::kEntry);

    AstList::Item* args_head = fn->args()->head();
    for (; args_head != NULL; args_head = args_head->next()) {
      entry->AddArg(Visit(args_head->value()));
    }

    Add(entry);

    VisitChildren(stmt);

    if (!current_block()->IsEnded()) {
      HInstruction* val = Add(HInstruction::kNil);
      HInstruction* end = Return(HInstruction::kReturn);
      end->AddArg(val);
    }

    return NULL;
  } else {
    HFunction* f = new HFunction(this, current_block(), stmt);
    work_queue_.Push(f);
    return Add(f);
  }
}


HInstruction* HGen::VisitAssign(AstNode* stmt) {
  HInstruction* rhs = Visit(stmt->rhs());

  if (stmt->lhs()->is(AstNode::kValue)) {
    AstValue* value = AstValue::Cast(stmt->lhs());

    if (value->slot()->is_stack()) {
      // No instruction is needed
      Assign(value->slot(), rhs);
    } else {
      Add(HInstruction::kStoreContext, value->slot())->AddArg(rhs);
    }
  } else if (stmt->lhs()->is(AstNode::kMember)) {
    HInstruction* property = Visit(stmt->lhs()->rhs());
    HInstruction* receiver = Visit(stmt->lhs()->lhs());

    Add(HInstruction::kStoreProperty)
        ->AddArg(receiver)
        ->AddArg(property)
        ->AddArg(rhs);
  } else {
    // TODO: Set error! Incorrect lhs
    abort();
  }

  return rhs;
}


HInstruction* HGen::VisitReturn(AstNode* stmt) {
  return Return(HInstruction::kReturn)->AddArg(Visit(stmt->lhs()));
}


HInstruction* HGen::VisitValue(AstNode* stmt) {
  AstValue* value = AstValue::Cast(stmt);
  ScopeSlot* slot = value->slot();
  if (slot->is_stack()) {
    HInstruction* i = current_block()->env()->At(slot);

    if (i != NULL && i->block() == current_block()) {
      // Local value
      return i;
    } else {
      HPhi* phi = CreatePhi(slot);
      if (i != NULL) phi->AddInput(i);

      // External value
      return Add(Assign(slot, phi));
    }
  } else {
    return Add(HInstruction::kLoadContext, slot);
  }
}


HInstruction* HGen::VisitIf(AstNode* stmt) {
  HBlock* t = CreateBlock();
  HBlock* f = CreateBlock();
  HInstruction* cond = Visit(stmt->lhs());

  Branch(HInstruction::kIf, t, f)->AddArg(cond);

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


HInstruction* HGen::VisitWhile(AstNode* stmt) {
  BreakContinueInfo* old = break_continue_info_;
  HBlock* start = CreateBlock();

  Goto(HInstruction::kGoto, start);

  // HBlock can't be join and branch at the same time
  set_current_block(CreateBlock());
  start->MarkLoop();
  start->Goto(HInstruction::kGoto, current_block());

  HInstruction* cond = Visit(stmt->lhs());

  HBlock* body = CreateBlock();
  HBlock* loop = CreateBlock();
  HBlock* end = CreateBlock();

  Branch(HInstruction::kIf, body, end)->AddArg(cond);

  set_current_block(body);
  break_continue_info_ = new BreakContinueInfo(this, end);

  Visit(stmt->rhs());

  while (break_continue_info_->continue_blocks()->length() > 0) {
    HBlock* next = break_continue_info_->continue_blocks()->Shift();
    Goto(HInstruction::kGoto, next);
    set_current_block(next);
  }
  Goto(HInstruction::kGoto, loop);
  loop->Goto(HInstruction::kGoto, start);

  // Next current block should not be a join
  set_current_block(break_continue_info_->GetBreak());

  // Restore break continue info
  break_continue_info_ = old;
}


HInstruction* HGen::VisitBreak(AstNode* stmt) {
  assert(break_continue_info_ != NULL);
  Goto(HInstruction::kGoto, break_continue_info_->GetBreak());
}


HInstruction* HGen::VisitContinue(AstNode* stmt) {
  assert(break_continue_info_ != NULL);
  Goto(HInstruction::kGoto, break_continue_info_->GetContinue());
}


HInstruction* HGen::VisitUnOp(AstNode* stmt) {
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
      HInstruction* ione = Visit(one);
      HInstruction* res = Visit(op->lhs());
      HInstruction* bin = Add(HInstruction::kBinOp)->AddArg(ione)->AddArg(res);

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
    return Add(HInstruction::kNot)->AddArg(Visit(op->lhs()));
  } else {
    UNEXPECTED
  }
}


HInstruction* HGen::VisitBinOp(AstNode* stmt) {
  BinOp* op = BinOp::Cast(stmt);
  HInstruction* res;

  if (!BinOp::is_bool_logic(op->subtype())) {
    res = Add(HInstruction::kBinOp)
        ->AddArg(Visit(op->lhs()))
        ->AddArg(Visit(op->rhs()));
  } else {
    HInstruction* lhs = Visit(op->lhs());
    HBlock* branch = CreateBlock();
    ScopeSlot* slot = current_block()->env()->logic_slot();

    Goto(HInstruction::kGoto, branch);
    set_current_block(branch);

    HBlock* t = CreateBlock();
    HBlock* f = CreateBlock();

    Branch(HInstruction::kIf, t, f)->AddArg(lhs);

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
    HPhi* phi =  current_block()->env()->PhiAt(slot);
    assert(phi != NULL);

    return phi;
  }

  res->ast(stmt);
  return res;
}


HInstruction* HGen::VisitObjectLiteral(AstNode* stmt) {
  HInstruction* res = Add(HInstruction::kAllocateObject);
  ObjectLiteral* obj = ObjectLiteral::Cast(stmt);

  AstList::Item* khead = obj->keys()->head();
  AstList::Item* vhead = obj->values()->head();
  for (; khead != NULL; khead = khead->next(), vhead = vhead->next()) {
    HInstruction* value = Visit(vhead->value());
    HInstruction* key = Visit(khead->value());

    Add(HInstruction::kStoreProperty)->AddArg(res)->AddArg(key)->AddArg(value);
  }

  return res;
}


HInstruction* HGen::VisitArrayLiteral(AstNode* stmt) {
  HInstruction* res = Add(HInstruction::kAllocateArray);

  AstList::Item* head = stmt->children()->head();
  for (uint64_t i = 0; head != NULL; head = head->next(), i++) {
    AstNode* index = new AstNode(AstNode::kNumber);
    char keystr[32];
    index->value(keystr);
    index->length(snprintf(keystr, sizeof(keystr), "%" PRIu64, i));

    HInstruction* key = Visit(index);
    HInstruction* value = Visit(head->value());

    // keystr is on-stack variable, nullify ast just for sanity
    key->ast(NULL);

    Add(HInstruction::kStoreProperty)->AddArg(res)->AddArg(key)->AddArg(value);
  }

  return res;
}


HInstruction* HGen::VisitMember(AstNode* stmt) {
  HInstruction* prop = Visit(stmt->rhs());
  HInstruction* recv = Visit(stmt->lhs());
  return Add(HInstruction::kLoadProperty)->AddArg(recv)->AddArg(prop);
}


HInstruction* HGen::VisitDelete(AstNode* stmt) {
  HInstruction* prop = Visit(stmt->lhs()->rhs());
  HInstruction* recv = Visit(stmt->lhs()->lhs());
  return Add(HInstruction::kDeleteProperty)->AddArg(recv)->AddArg(prop);
}


HInstruction* HGen::VisitCall(AstNode* stmt) {
  FunctionLiteral* fn = FunctionLiteral::Cast(stmt);

  // handle __$gc() and __$trace() calls
  if (fn->variable()->is(AstNode::kValue)) {
    AstNode* name = AstValue::Cast(fn->variable())->name();
    if (name->length() == 5 && strncmp(name->value(), "__$gc", 5) == 0) {
      return Add(HInstruction::kCollectGarbage);
    } else if (name->length() == 8 &&
               strncmp(name->value(), "__$trace", 8) == 0) {
      return Add(HInstruction::kGetStackTrace);
    }
  }

  HInstruction* receiver = NULL;

  if (fn->args()->length() > 0 &&
      fn->args()->head()->value()->is(AstNode::kSelf)) {
    receiver = Visit(fn->variable()->lhs());
  }

  HInstructionList args;

  AstList::Item* item = fn->args()->head();
  for (; item != NULL; item = item->next()) {
    if (item->value()->is(AstNode::kSelf)) {
      assert(receiver != NULL);
      args.Push(receiver);
    } else {
      args.Push(Visit(item->value()));
    }
  }

  HInstruction* var;
  if (fn->args()->length() > 0 &&
      fn->args()->head()->value()->is(AstNode::kSelf)) {
    assert(fn->variable()->is(AstNode::kMember));

    HInstruction* property = Visit(fn->variable()->rhs());

    var = Add(HInstruction::kLoadProperty)->AddArg(receiver)->AddArg(property);
  } else {
    var = Visit(fn->variable());
  }
  HInstruction* call = CreateInstruction(HInstruction::kCall)->AddArg(var);

  HInstructionList::Item* ihead = args.head();
  for (; ihead != NULL; ihead = ihead->next()) {
    call->AddArg(ihead->value());
  }

  return Add(call);
}


HInstruction* HGen::VisitTypeof(AstNode* stmt) {
  return Add(HInstruction::kTypeof)->AddArg(Visit(stmt->lhs()));
}


HInstruction* HGen::VisitKeysof(AstNode* stmt) {
  return Add(HInstruction::kKeysof)->AddArg(Visit(stmt->lhs()));
}

HInstruction* HGen::VisitSizeof(AstNode* stmt) {
  return Add(HInstruction::kSizeof)->AddArg(Visit(stmt->lhs()));
}


// Literals


HInstruction* HGen::VisitLiteral(AstNode* stmt) {
  HInstruction* i = Add(HInstruction::kLiteral, root_.Put(stmt));

  i->ast(stmt);

  return i;
}


HInstruction* HGen::VisitNumber(AstNode* stmt) {
  return VisitLiteral(stmt);
}


HInstruction* HGen::VisitNil(AstNode* stmt) {
  return Add(HInstruction::kNil);
}


HInstruction* HGen::VisitTrue(AstNode* stmt) {
  return VisitLiteral(stmt);
}


HInstruction* HGen::VisitFalse(AstNode* stmt) {
  return VisitLiteral(stmt);
}


HInstruction* HGen::VisitString(AstNode* stmt) {
  return VisitLiteral(stmt);
}


HInstruction* HGen::VisitProperty(AstNode* stmt) {
  return VisitLiteral(stmt);
}


HBlock::HBlock(HGen* g) : id(g->block_id()),
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


HInstruction* HBlock::Assign(ScopeSlot* slot, HInstruction* value) {
  assert(value != NULL);

  value->slot(slot);
  env()->Set(slot, value);

  return value;
}


void HBlock::AddPredecessor(HBlock* b) {
  assert(pred_count_ < 2);
  pred_[pred_count_++] = b;

  if (pred_count_ == 1) {
    // Fast path - copy environment
    env()->Copy(b->env());
    return;
  }

  for (int i = 0; i < b->env()->stack_slots(); i++) {
    HInstruction* curr = b->env()->At(i);
    if (curr == NULL) continue;

    HInstruction* old = this->env()->At(i);

    // Value already present in block
    if (old != NULL) {
      HPhi* phi = this->env()->PhiAt(i);

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


void HBlock::MarkLoop() {
  loop_ = true;

  // Create phi for every possible value (except logic_slot)
  for (int i = 0; i < env()->stack_slots() - 1; i++) {
    ScopeSlot* slot = new ScopeSlot(ScopeSlot::kStack);
    slot->index(i);

    HInstruction* old = env()->At(i);
    HPhi* phi = CreatePhi(slot);
    if (old != NULL) phi->AddInput(old);

    Add(Assign(slot, phi));
  }
}


void HGen::Replace(HInstruction* o, HInstruction* n) {
  HInstructionList::Item* head = o->uses()->head();
  for (; head != NULL; head = head->next()) {
    HInstruction* use = head->value();

    use->ReplaceArg(o, n);
  }
}


void HBlock::Remove(HInstruction* instr) {
  HInstructionList::Item* head = instructions_.head();
  for (; head != NULL; head = head->next()) {
    HInstruction* i = head->value();

    if (i == instr) {
      instructions_.Remove(head);
      break;
    }
  }

  instr->Remove();
}


void HBlock::PruneHPhis() {
  HPhiList queue_;

  HPhiList::Item* head = phis_.head();
  for (; head != NULL; head = head->next()) {
    queue_.Push(head->value());
  }

  while (queue_.length() > 0) {
    HPhi* phi = queue_.Shift();

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
      if (phi->InputAt(i)->Is(HInstruction::kPhi)) {
        queue_.Push(HPhi::Cast(phi->InputAt(i)));
      }
    }
  }
}


HEnvironment::HEnvironment(int stack_slots) : stack_slots_(stack_slots + 1) {
  // ^^ NOTE: One stack slot is reserved for bool logic binary operations
  logic_slot_ = new ScopeSlot(ScopeSlot::kStack);
  logic_slot_->index(stack_slots);

  instructions_ = reinterpret_cast<HInstruction**>(Zone::current()->Allocate(
      sizeof(*instructions_) * stack_slots_));
  memset(instructions_, 0, sizeof(*instructions_) * stack_slots_);

  phis_ = reinterpret_cast<HPhi**>(Zone::current()->Allocate(
      sizeof(*phis_) * stack_slots_));
  memset(phis_, 0, sizeof(*phis_) * stack_slots_);
}


void HEnvironment::Copy(HEnvironment* from) {
  memcpy(instructions_,
         from->instructions_,
         sizeof(*instructions_) * stack_slots_);
  memcpy(phis_,
         from->phis_,
         sizeof(*phis_) * stack_slots_);
}


BreakContinueInfo::BreakContinueInfo(HGen *g, HBlock* end) : g_(g), brk_(end) {
}


HBlock* BreakContinueInfo::GetContinue() {
  HBlock* b = g_->CreateBlock();

  continue_blocks()->Push(b);

  return b;
}


HBlock* BreakContinueInfo::GetBreak() {
  HBlock* b = g_->CreateBlock();
  brk_->Goto(HInstruction::kGoto, b);
  brk_ = b;

  return b;
}

} // namespace hir
} // namespace internal
} // namespace candor
