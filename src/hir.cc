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
                                         successors_count_(0) {
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
}


HIRValue::HIRValue(HIRBasicBlock* block, ScopeSlot* slot) : block_(block),
                                                            slot_(slot) {
  block->AddValue(this);
}


HIR::HIR(Heap* heap, AstNode* node) : Visitor(kPreorder),
                                      current_block_(NULL),
                                      root_(heap) {
  Visit(node);
}


HIRValue* HIR::CreateValue(ScopeSlot* slot) {
  assert(current_block() != NULL);
  HIRValue* value = new HIRValue(current_block(), slot);

  slot->hir(value);

  return value;
}


HIRValue* HIR::GetValue(ScopeSlot* slot) {
  if (slot->hir() == NULL) CreateValue(slot);

  return slot->hir();
}


HIRBasicBlock* HIR::CreateBlock() {
  return new HIRBasicBlock(this);
}


void HIR::AddInstruction(HIRInstruction* instr) {
  assert(current_block() != NULL);

  instr->Init(current_block());
  current_block()->instructions()->Push(instr);
}


HIRValue* HIR::GetLastInstructionResult() {
  assert(current_block() != NULL);
  assert(current_block()->instructions()->length() != 0);
  return current_block()->instructions()->tail()->value()->GetResult();
}


AstNode* HIR::VisitFunction(AstNode* stmt) {
  if (current_block() == NULL) {
    set_current_block(CreateBlock());
    VisitChildren(stmt);
  } else {
    // TODO: Allocate instruction for non-main functions
  }

  return stmt;
}


AstNode* HIR::VisitAssign(AstNode* stmt) {
  if (stmt->lhs()->is(AstNode::kValue)) {
    AstValue* value = AstValue::Cast(stmt->lhs());
    HIRValue* lhs = CreateValue(value->slot());

    Visit(stmt->rhs());
    HIRValue* rhs = GetLastInstructionResult();

    if (value->slot()->is_stack()) {
      AddInstruction(new HIRStoreLocal(lhs, rhs));
    } else {
      AddInstruction(new HIRStoreContext(lhs, rhs));
    }
  } else {
    Visit(stmt->lhs());
    HIRValue* lhs = GetLastInstructionResult();
    Visit(stmt->rhs());
    HIRValue* rhs = GetLastInstructionResult();

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


AstNode* HIR::VisitObjectLiteral(AstNode* node) {
  return node;
}


AstNode* HIR::VisitArrayLiteral(AstNode* node) {
  return node;
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
