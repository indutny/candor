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
  values()->Push(value);
}


void HIRBasicBlock::AddPredecessor(HIRBasicBlock* block) {
  assert(predecessors_count() < 2);
  predecessors_[predecessors_count_++] = block;
}


void HIRBasicBlock::AddSuccessor(HIRBasicBlock* block) {
  assert(successors_count() < 2);
  successors_[successors_count_++] = block;
  block->AddPredecessor(this);
}


void HIRBasicBlock::Goto(HIRBasicBlock* block) {
  // Connect graph nodes
  AddSuccessor(block);

  // Add goto instruction and finalize block
  HIRGoto* instr = new HIRGoto();
  instructions()->Push(instr);
  instr->Init(this);
  finished(true);
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

  // Print instructions
  InstructionList::Item* item = instructions()->head();
  while (item != NULL) {
    item->value()->Print(p);
    item = item->next();
    p->Print(" ");
  }

  // Print successors' ids
  p->Print("{");
  if (successors_count() == 2) {
    p->Print("%d+%d", successors()[0]->id(), successors()[1]->id());
  } else if (successors_count() == 1) {
    p->Print("%d", successors()[0]->id());
  }
  p->Print("}]");

  // Print successors
  if (successors_count() == 2) {
    if (successors()[0]->IsPrintable()) {
      p->Print(" ");
      successors()[0]->Print(p);
    }
    if (successors()[1]->IsPrintable()) {
      p->Print(" ");
      successors()[1]->Print(p);
    }
  } else if (successors_count() == 1) {
    if (successors()[0]->IsPrintable()) {
      p->Print(" ");
      successors()[0]->Print(p);
    }
  }
}


HIRBasicBlock::Phi::Phi(HIRBasicBlock* block) {
  result_ = new HIRValue(block);
}


HIRValue::HIRValue(HIRBasicBlock* block) : block_(block), prev_def_(NULL) {
  slot_ = new ScopeSlot(ScopeSlot::kRegister);
  block->AddValue(this);
  id_ = block->hir()->get_variable_index();
}


HIRValue::HIRValue(HIRBasicBlock* block, ScopeSlot* slot) : block_(block),
                                                            slot_(slot),
                                                            prev_def_(NULL) {
  block->AddValue(this);
  id_ = block->hir()->get_variable_index();
}


void HIRValue::Print(PrintBuffer* p) {
  p->Print("@[%d ", id());
  slot()->Print(p);
  p->Print("]");
}


HIR::HIR(Heap* heap, AstNode* node) : Visitor(kPreorder),
                                      root_(heap),
                                      block_index_(0),
                                      variable_index_(0),
                                      print_map_(NULL) {
  root_block_ = CreateBlock();
  current_block_ = root_block_;
  Visit(node);
}


HIRValue* HIR::CreateValue(ScopeSlot* slot) {
  assert(current_block() != NULL);
  HIRValue* value = new HIRValue(current_block(), slot);

  HIRValue* previous = slot->hir();
  // Find appropriate value
  while (previous != NULL) {
    // Traverse blocks to the root, to check
    // if variable was used in predecessor
    HIRBasicBlock* block = current_block();
    while (block != NULL && previous->block() != block) {
      block = block->predecessors()[0];
    }
    previous = previous->prev_def();
  }

  if (previous != NULL) {
    // Link variables
    value->prev_def(previous);
    previous->next_defs()->Push(value);
  }

  slot->hir(value);

  return value;
}


HIRValue* HIR::GetValue(ScopeSlot* slot) {
  if (slot->hir() == NULL || slot->hir()->block() != current_block()) {
    // Slot wasn't used or was used in some of predecessor blocks
    // Insert new one HIRValue in the current block
    CreateValue(slot);
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

  // Check both block's values and insert phis for those
  // that was modified (used) in both blocks
  HIRValueList::Item* litem = left->values()->head();
  HIRValueList::Item* ritem = right->values()->head();
  for (;litem != NULL; litem = litem->next()) {
    // Check only final values
    if (litem->value()->next_defs()->length() != 0) continue;

    HIRBasicBlock::Phi* phi = NULL;

    for (;ritem != NULL; ritem = ritem->next()) {
      // Check only final values
      if (ritem->value()->next_defs()->length() != 0) continue;

      if (litem->value()->slot() == ritem->value()->slot()) {
        // Lazily allocate phi
        if (phi == NULL) {
          phi = new HIRBasicBlock::Phi(join);
          join->phis()->Push(phi);
        }

        phi->incoming()->Push(litem->value());
      }
    }
  }
}


void HIR::AddInstruction(HIRInstruction* instr) {
  assert(current_block() != NULL);
  assert(current_block()->finished() == false);

  current_block()->instructions()->Push(instr);
  instr->Init(current_block());
}


void HIR::Finish(HIRInstruction* instr) {
  assert(current_block() != NULL);
  AddInstruction(instr);
  current_block()->finished(true);
}


HIRValue* HIR::GetLastInstructionResult() {
  assert(current_block() != NULL);
  assert(current_block()->instructions()->length() != 0);
  return current_block()->instructions()->tail()->value()->GetResult();
}


void HIR::Print(char* buffer, uint32_t size) {
  PrintMap map;

  print_map(&map);

  PrintBuffer p(buffer, size);
  root_block()->Print(&p);
  p.Finalize();

  print_map(NULL);
}


AstNode* HIR::VisitFunction(AstNode* stmt) {
  if (current_block() == root_block()) {
    VisitChildren(stmt);
  } else {
    // TODO: Allocate instruction for non-main functions
  }

  return stmt;
}


AstNode* HIR::VisitAssign(AstNode* stmt) {
  if (stmt->lhs()->is(AstNode::kValue)) {
    Visit(stmt->rhs());
    HIRValue* rhs = GetLastInstructionResult();

    AstValue* value = AstValue::Cast(stmt->lhs());
    HIRValue* lhs = CreateValue(value->slot());

    if (value->slot()->is_stack()) {
      AddInstruction(new HIRStoreLocal(lhs, rhs));
    } else {
      AddInstruction(new HIRStoreContext(lhs, rhs));
    }
  } else {
    Visit(stmt->rhs());
    HIRValue* rhs = GetLastInstructionResult();

    Visit(stmt->lhs());
    HIRValue* lhs = GetLastInstructionResult();

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
  // Get value of condition
  Visit(node->lhs());

  HIRBasicBlock* on_true = CreateBlock();
  HIRBasicBlock* on_false = CreateBlock();
  HIRBasicBlock* join = NULL;
  HIRBranchBool* branch = new HIRBranchBool(GetLastInstructionResult(),
                                            on_true,
                                            on_false);
  Finish(branch);

  set_current_block(on_true);
  Visit(node->rhs());

  AstList::Item* else_body = node->children()->head()->next()->next();
  if (else_body != NULL) {
    // Visit else body and create additional `join` block
    set_current_block(on_false);
    Visit(else_body->value());
  }

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
