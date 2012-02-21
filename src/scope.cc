#include "scope.h"
#include "ast.h" // AstNode, AstList

#include <assert.h>

namespace dotlang {

Scope::Scope(ScopeAnalyze* a, Type type) : a_(a), type_(type) {
  parent_ = a_->scope_;
  a_->scope_ = this;

  stack_count_ = 0;
  context_count_ = 0;

  if (parent_ != NULL && type_ != kFunction) {
    stack_index_ = parent_->stack_index_;
    context_index_ = parent_->context_index_;
  } else {
    stack_index_ = 0;
    context_index_ = 0;
  }

  // Increase depth when entering function
  // depth_ = 0 is for global object
  depth_ = parent_ == NULL ? 1 : parent_->depth_;
  if (type_ == kFunction) depth_++;
}


Scope::~Scope() {
  assert(stack_count_ >= 0);
  assert(context_count_ >= 0);

  // Assign indexes to each stack item
  Enumerate(ScopeSlot::Enumerate);

  // Lift up stack and context sizes
  if (type_ != kFunction && parent_ != NULL) {
    parent_->stack_count_ += stack_count_;
    parent_->context_count_ += context_count_;

    parent_->stack_index_ = stack_index_;
    parent_->context_index_ = context_index_;
  }

  a_->scope_ = parent_;
}


ScopeSlot* Scope::GetSlot(const char* name,
                          uint32_t length) {
  ScopeSlot* slot = Get(name, length);

  if (slot == NULL) {
    slot = new ScopeSlot(ScopeSlot::kStack);
    Set(name, length, slot);

    stack_count_++;
  }

  return slot;
}


void Scope::MoveToContext(const char* name, uint32_t length) {
  ScopeSlot* slot = Get(name, length);

  if (slot == NULL) {
    // Variable wasn't used here so far
    // Mark it as context variable in parent scope
    // Bubble up until parent will be found
    Scope* scope = parent_;
    int32_t depth = type_ == kFunction;
    while (scope != NULL) {
      if ((slot = scope->Get(name, length)) != NULL &&
          (slot->is_stack() || slot->depth() == 0))  {
        scope->MoveToContext(name, length);
        break;
      }

      if (scope->type_ == kFunction) depth++;

      scope = scope->parent_;
    }

    if (scope == NULL) depth = -1;

    slot = new ScopeSlot(ScopeSlot::kContext, depth);
    Set(name, length, slot);
  } else if (slot->is_stack()) {
    // Variable was stored in stack, but should be moved into context
    slot->type_ = ScopeSlot::kContext;
    stack_count_--;
    context_count_++;
  }
}


void ScopeSlot::Enumerate(void* scope, ScopeSlot* slot) {
  Scope* scope_ = reinterpret_cast<Scope*>(scope);

  if (slot->is_stack()) {
    slot->index_ = scope_->stack_index_++;
  } else if (slot->is_context()) {
    slot->index_ = scope_->context_index_++;
  } else {
    assert(0 && "Unreachable");
  }
}


void Scope::Analyze(AstNode* ast) {
  ScopeAnalyze a(ast);
}


ScopeAnalyze::ScopeAnalyze(AstNode* ast) : Visitor(kBreadthFirst),
                                           ast_(ast),
                                           scope_(NULL) {
  Visit(ast);
}


AstNode* ScopeAnalyze::VisitFunction(AstNode* node) {
  FunctionLiteral* fn = FunctionLiteral::Cast(node);

  // Put variable into outer scope
  if (fn->variable() != NULL) {
    fn->variable(new AstValue(scope(), fn->variable()));
  }

  Scope scope(this, node == ast_ ? Scope::kBlock : Scope::kFunction);

  VisitChildren(node);

  node->SetScope(&scope);

  return node;
}


AstNode* ScopeAnalyze::VisitBlock(AstNode* node) {
  Scope scope(this, Scope::kBlock);
  VisitChildren(node);
  return node;
}


AstNode* ScopeAnalyze::VisitScopeDecl(AstNode* node) {
  AstList::Item* child = node->children()->head();
  while (child != NULL) {
    scope_->MoveToContext(child->value()->value(), child->value()->length());
    child = child->next();
  }
  return node;
}


AstNode* ScopeAnalyze::VisitName(AstNode* node) {
  return new AstValue(scope(), node);
}

} // namespace dotlang
