#include "hir.h"
#include "hir-instructions.h"
#include "visitor.h" // Visitor
#include "ast.h" // AstNode
#include "utils.h" // List

#include <stdlib.h> // NULL
#include <stdint.h> // int64_t
#include <assert.h> // assert

namespace candor {
namespace internal {

HIRBasicBlock::HIRBasicBlock(HIR* hir) : hir_(hir),
                                         predecessors_count_(0),
                                         successors_count_(0),
                                         finished_(false),
                                         id_(hir->get_block_index()) {
  predecessors_[0] = NULL;
  predecessors_[1] = NULL;
  successors_[0] = NULL;
  successors_[1] = NULL;
}


void HIRBasicBlock::AddValue(HIRValue* value) {
  // If value is already in values list - remove previous
  HIRValueList::Item* item = values()->head();
  for (; item != NULL; item = item->next()) {
    if (item->value()->slot() == value->slot()) {
      values()->Remove(item);
    }
  }
  values()->Push(value);
}


void HIRBasicBlock::AddPredecessor(HIRBasicBlock* block) {
  assert(predecessors_count() < 2);
  predecessors()[predecessors_count_++] = block;

  // Propagate used values from predecessor to current block
  HIRValueList::Item* item = block->values()->head();
  for (; item != NULL; item = item->next()) {
    HIRValue* value = item->value();

    if (value == NULL) continue;

    // If value is already in current block - insert phi!
    if (value->slot()->hir()->current_block() == this &&
        value->slot()->hir() != value) {
      HIRPhi* phi = HIRPhi::Cast(value->slot()->hir());
      if (!phi->is_phi()) {
        phi = new HIRPhi(this, value->slot()->hir());

        value->slot()->hir(phi);
        values()->Push(phi);

        // Push to block's and global phi list
        phis()->Push(phi);
        hir()->phis()->Push(phi);
        hir()->values()->Push(phi);
      }

      phi->inputs()->Push(value);
    } else {
      // Otherwise put value to the list
      values()->Push(value);
      // And associated with a current block
      value->current_block(this);
      value->slot()->hir(value);
    }
  }
}


void HIRBasicBlock::AddSuccessor(HIRBasicBlock* block) {
  assert(successors_count() < 2);
  successors()[successors_count_++] = block;
  block->AddPredecessor(this);
}


void HIRBasicBlock::Goto(HIRBasicBlock* block) {
  if (finished()) return;

  // Connect graph nodes
  AddSuccessor(block);

  // Find previous instruction to
  // insert Goto in the instructions chain and to
  // detemine optimal instruction index
  HIRInstruction* prev = NULL;
  int id = 0;
  HIRBasicBlock* current = this;
  while (current != NULL) {
    if (current->instructions()->length() > 0) {
      prev = current->last_instruction();
      id = prev->id();
      break;
    }

    // Go to the predecessor block
    current = current->predecessors()[0];
  }

  // Add goto instruction and finalize block
  HIRGoto* instr = new HIRGoto();
  instructions()->Push(instr);
  instr->Init(this, id);
  finished(true);

  if (prev != NULL) {
    // Previous instruction found - insert Goto in the linked list
    instr->prev(prev);
    instr->next(prev->next());

    if (prev->next() != NULL) prev->next()->prev(instr);
    prev->next(instr);

    if (prev == hir()->last_instruction()) hir()->last_instruction(instr);
  } else {
    // No previous instruction - goto should become the first instruction
    instr->prev(prev);
    instr->next(hir()->first_instruction());

    if (hir()->first_instruction() != NULL) {
      hir()->first_instruction()->prev(instr);
    }
    hir()->first_instruction(instr);
    if (hir()->last_instruction() == NULL) hir()->last_instruction(instr);
  }
}


bool HIRBasicBlock::Dominates(HIRBasicBlock* block) {
  while (block != NULL) {
    if (block == this) return true;
    if (block->predecessors_count() != 1) return false;
    block = block->predecessors()[0];
  }

  return false;
}


bool HIRBasicBlock::IsPrintable() {
  return hir()->print_map()->Get(NumberKey::New(id())) == NULL;
}


void HIRBasicBlock::MarkPrinted() {
  int value;
  hir()->print_map()->Set(NumberKey::New(id()), &value);
}


void HIRBasicBlock::Print(PrintBuffer* p) {
  // Avoid loops and double prints
  MarkPrinted();

  p->Print("[Block#%d ", id());

  // Print values
  {
    HIRValueList::Item* item = values()->head();
    p->Print("{");
    while (item != NULL) {
      p->Print("%d", item->value()->id());
      item = item->next();
      if (item != NULL) p->Print(",");
    }
    p->Print("} ");
  }

  // Print phis
  {
    HIRPhiList::Item* item = phis()->head();
    while (item != NULL) {
      item->value()->Print(p);
      item = item->next();
      p->Print(" ");
    }
  }

  // Print instructions
  {
    HIRInstructionList::Item* item = instructions()->head();
    while (item != NULL) {
      item->value()->Print(p);
      item = item->next();
      p->Print(" ");
    }
  }

  // Print predecessors' ids
  if (predecessors_count() == 2) {
    p->Print("[%d,%d]", predecessors()[0]->id(), predecessors()[1]->id());
  } else if (predecessors_count() == 1) {
    p->Print("[%d]", predecessors()[0]->id());
  } else {
    p->Print("[]");
  }

  p->Print(">*>");

  // Print successors' ids
  if (successors_count() == 2) {
    p->Print("[%d,%d]", successors()[0]->id(), successors()[1]->id());
  } else if (successors_count() == 1) {
    p->Print("[%d]", successors()[0]->id());
  } else {
    p->Print("[]");
  }

  p->Print("]\n");

  // Print successors
  if (successors_count() == 2) {
    if (successors()[0]->IsPrintable()) successors()[0]->Print(p);
    if (successors()[1]->IsPrintable()) successors()[1]->Print(p);
  } else if (successors_count() == 1) {
    if (successors()[0]->IsPrintable()) successors()[0]->Print(p);
  }
}


HIRPhi::HIRPhi(HIRBasicBlock* block, HIRValue* value)
    : HIRValue(block, value->slot()) {
  type_ = kPhi;
  inputs()->Push(value);
}


void HIRPhi::Print(PrintBuffer* p) {
  p->Print("@[");
  HIRValueList::Item* item = inputs()->head();
  while (item != NULL) {
    p->Print("%d", item->value()->id());
    item = item->next();
    if (item != NULL) p->Print(",");
  }
  p->Print("]:%d", id());
}


HIRValue::HIRValue(HIRBasicBlock* block) : type_(kNormal),
                                           block_(block),
                                           current_block_(block),
                                           prev_def_(NULL),
                                           operand_(NULL) {
  slot_ = new ScopeSlot(ScopeSlot::kStack);
  Init();
}


HIRValue::HIRValue(HIRBasicBlock* block, ScopeSlot* slot)
    : type_(kNormal),
      block_(block),
      current_block_(block),
      prev_def_(NULL),
      operand_(NULL),
      slot_(slot) {
  Init();
}


void HIRValue::Init() {
  block()->AddValue(this);
  id_ = block()->hir()->get_variable_index();

  live_range()->start = -1;
  live_range()->end = -1;
}


void HIRValue::Print(PrintBuffer* p) {
  if (prev_def() == NULL) {
    p->Print("*[%d ", id());
  } else {
    p->Print("*[%d>%d ", prev_def()->id(), id());
  }
    slot()->Print(p);
    p->Print("]");
}


HIR::HIR(Heap* heap, AstNode* node) : Visitor(kPreorder),
                                      root_(heap),
                                      first_instruction_(NULL),
                                      last_instruction_(NULL),
                                      block_index_(0),
                                      variable_index_(0),
                                      instruction_index_(1),
                                      print_map_(NULL) {
  work_list_.Push(new HIRFunction(node, CreateBlock()));

  HIRFunction* fn;
  while ((fn = work_list_.Shift()) != NULL) {
    root_block_ = fn->block();
    roots()->Push(fn->block());
    set_current_block(fn->block());
    Visit(fn->node());
  }
}


HIRValue* HIR::FindPredecessorValue(ScopeSlot* slot) {
  assert(current_block() != NULL);

  // Find appropriate value
  HIRValue* previous = slot->hir();
  while (previous != NULL) {
    // Traverse blocks to the root, to check
    // if variable was used in predecessor
    HIRBasicBlock* block = current_block();
    while (block != NULL && previous->block() != block) {
      block = block->predecessors()[0];
    }
    if (block != NULL) break;
    previous = previous->prev_def();
  }

  return previous;
}


HIRValue* HIR::CreateValue(HIRBasicBlock* block, ScopeSlot* slot) {
  HIRValue* value = new HIRValue(block, slot);
  HIRValue* previous = FindPredecessorValue(slot);

  // Link with previous
  if (previous != NULL) {
    value->prev_def(previous);
    previous->next_defs()->Push(value);
  }

  slot->hir(value);

  // Push value to the values list
  values()->Push(value);

  return value;
}


HIRValue* HIR::CreateValue(HIRBasicBlock* block) {
  return CreateValue(block, new ScopeSlot(ScopeSlot::kStack));
}


HIRValue* HIR::CreateValue(ScopeSlot* slot) {
  return CreateValue(current_block(), slot);
}


HIRValue* HIR::GetValue(ScopeSlot* slot) {
  assert(current_block() != NULL);

  // Slot was used - find one in our branch
  HIRValue* previous = FindPredecessorValue(slot);

  // Lazily create new variable
  if (previous == NULL) {
    // Slot wasn't used in HIR yet
    // Insert new one HIRValue in the current block
    CreateValue(slot);
  } else {
    if (previous != slot->hir()) {
      // Create slot and link variables
      HIRValue* value = new HIRValue(current_block(), slot);

      // Link with previous
      if (slot->hir() != NULL) {
        value->prev_def(previous);
        previous->next_defs()->Push(value);
      }

      slot->hir(value);

      // Push value to the values list
      values()->Push(value);
    } else {
      slot->hir()->current_block(current_block());
    }
  }

  return slot->hir();
}


HIRBasicBlock* HIR::CreateBlock() {
  return new HIRBasicBlock(this);
}


HIRBasicBlock* HIR::CreateJoin(HIRBasicBlock* left, HIRBasicBlock* right) {
  HIRBasicBlock* join = CreateBlock();

  left->Goto(join);
  right->Goto(join);

  return join;
}


void HIR::AddInstruction(HIRInstruction* instr) {
  assert(current_block() != NULL);
  instr->Init(current_block(), get_instruction_index());

  if (current_block()->finished()) return;

  // Link instructions together
  if (last_instruction() != NULL) {
    last_instruction()->next(instr);
    instr->prev(last_instruction());
  }
  last_instruction(instr);

  current_block()->instructions()->Push(instr);
}


void HIR::Finish(HIRInstruction* instr) {
  assert(current_block() != NULL);
  AddInstruction(instr);
  current_block()->finished(true);
}


HIRValue* HIR::GetValue(AstNode* node) {
  Visit(node);

  if (current_block() == NULL ||
      current_block()->instructions()->length() == 0) {
    return CreateValue(root()->Put(new AstNode(AstNode::kNil)));
  } else {
    return current_block()->instructions()->tail()->value()->GetResult();
  }
}


void HIR::Print(char* buffer, uint32_t size) {
  PrintMap map;

  PrintBuffer p(buffer, size);
  print_map(&map);

  ZoneList<HIRBasicBlock*>::Item* item = roots()->head();
  for (; item != NULL; item = item->next()) {
    item->value()->Print(&p);
  }

  print_map(NULL);
  p.Finalize();
}


AstNode* HIR::VisitFunction(AstNode* stmt) {
  FunctionLiteral* fn = FunctionLiteral::Cast(stmt);

  if (current_block() == root_block() &&
      current_block()->instructions()->length() == 0) {
    HIRInstruction* entry = new HIREntry();
    if (first_instruction() == NULL) first_instruction(entry);

    AddInstruction(entry);
    if (fn->context_slots() > 0) {
      AddInstruction(new HIRAllocateContext(fn->context_slots()));
    }

    VisitChildren(stmt);

    AddInstruction(new HIRReturn(CreateValue(
            root()->Put(new AstNode(AstNode::kNil)))));
  } else {
    HIRBasicBlock* block = CreateBlock();
    AddInstruction(new HIRAllocateFunction(block, fn->args()->length()));

    work_list_.Push(new HIRFunction(stmt, block));
  }

  return stmt;
}


AstNode* HIR::VisitAssign(AstNode* stmt) {
  if (stmt->lhs()->is(AstNode::kValue)) {
    HIRValue* rhs = GetValue(stmt->rhs());

    AstValue* value = AstValue::Cast(stmt->lhs());
    HIRValue* lhs = CreateValue(value->slot());

    if (value->slot()->is_stack()) {
      AddInstruction(new HIRStoreLocal(lhs, rhs));
    } else {
      AddInstruction(new HIRStoreContext(lhs, rhs));
    }
  } else {
    HIRValue* rhs = GetValue(stmt->rhs());
    HIRValue* lhs = GetValue(stmt->lhs());

    AddInstruction(new HIRStoreProperty(lhs, rhs));
  }

  return stmt;
}


AstNode* HIR::VisitValue(AstNode* node) {
  AstValue* value = AstValue::Cast(node);
  if (value->slot()->is_stack()) {
    AddInstruction(new HIRLoadLocal(GetValue(value->slot())));
  } else {
    AddInstruction(new HIRLoadContext(GetValue(value->slot())));
  }
  return node;
}


void HIR::VisitRootValue(AstNode* node) {
  AddInstruction(new HIRLoadRoot(CreateValue(root()->Put(node))));
}


AstNode* HIR::VisitIf(AstNode* node) {
  HIRBasicBlock* on_true = CreateBlock();
  HIRBasicBlock* on_false = CreateBlock();
  HIRBasicBlock* join = NULL;
  HIRBranchBool* branch = new HIRBranchBool(GetValue(node->lhs()),
                                            on_true,
                                            on_false);
  Finish(branch);

  set_current_block(on_true);
  Visit(node->rhs());
  on_true = current_block();

  AstList::Item* else_body = node->children()->head()->next()->next();
  set_current_block(on_false);

  if (else_body != NULL) {
    // Visit else body and create additional `join` block
    Visit(else_body->value());
  } else {
    AddInstruction(new HIRNop());
  }

  on_false = current_block();

  set_current_block(CreateJoin(on_true, on_false));

  return node;
}


AstNode* HIR::VisitWhile(AstNode* node) {
  return node;
}


AstNode* HIR::VisitMember(AstNode* node) {
  return node;
}


AstNode* HIR::VisitCall(AstNode* stmt) {
  FunctionLiteral* fn = FunctionLiteral::Cast(stmt);

  Visit(fn->variable());
  HIRCall* call = new HIRCall(GetValue(fn->variable()));

  AstList::Item* item = fn->args()->head();
  for (; item != NULL; item = item->next()) {
    call->AddArg(GetValue(item->value()));
  }

  AddInstruction(call);

  return stmt;
}


void HIR::VisitGenericObject(AstNode* node) {
  int size = node->children()->length();
  HIRAllocateObject::ObjectKind kind;
  switch (node->type()) {
   case AstNode::kObjectLiteral:
    kind = HIRAllocateObject::kObject;
    break;
   case AstNode::kArrayLiteral:
    kind = HIRAllocateObject::kArray;
    break;
   default: UNEXPECTED break;
  }

  // Create object
  AddInstruction(new HIRAllocateObject(kind, size));

  // TODO: Put properties into it
}


AstNode* HIR::VisitReturn(AstNode* node) {
  HIRValue* result = NULL;
  if (node->lhs() != NULL) result = GetValue(node->lhs());

  if (result == NULL) {
    result = CreateValue(root()->Put(new AstNode(AstNode::kNil)));
  }
  Finish(new HIRReturn(result));

  return node;
}


AstNode* HIR::VisitClone(AstNode* node) {
  return node;
}


AstNode* HIR::VisitDelete(AstNode* node) {
  return node;
}


AstNode* HIR::VisitBreak(AstNode* node) {
  return node;
}


AstNode* HIR::VisitContinue(AstNode* node) {
  return node;
}


AstNode* HIR::VisitTypeof(AstNode* node) {
  return node;
}


AstNode* HIR::VisitSizeof(AstNode* node) {
  return node;
}


AstNode* HIR::VisitKeysof(AstNode* node) {
  return node;
}


AstNode* HIR::VisitUnOp(AstNode* node) {
  return node;
}


AstNode* HIR::VisitBinOp(AstNode* node) {
  return node;
}

} // namespace internal
} // namespace candor
