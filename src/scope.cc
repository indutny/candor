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
    ScopeSlot* source = NULL;

    // If current scope is a function's scope and it doesn't have
    // variable that we're seeking for - lookup depth should be set to 1
    int32_t depth = type_ == kFunction;
    while (scope != NULL) {
      if ((slot = scope->Get(name, length)) != NULL &&
          (slot->is_stack() || slot->depth() == 0))  {
        // Move parent's slot to context if it is stack allocated
        if (slot->is_stack()) scope->MoveToContext(name, length);

        source = slot;
        break;
      }

      // Increase depth as we go further
      // Contexts are allocated only in functions, so we should not
      // increase depth when traversing block scopes
      if (scope->type_ == kFunction) depth++;

      scope = scope->parent_;
    }

    if (scope == NULL) depth = -1;

    slot = new ScopeSlot(ScopeSlot::kContext, depth);
    Set(name, length, slot);

    // Store reference in original slot
    if (source != NULL) source->uses()->Push(slot);
  } else if (slot->is_stack()) {
    // Variable was stored in stack, but should be moved into context
    slot->type_ = ScopeSlot::kContext;
    stack_count_--;
    context_count_++;
  }
}


void ScopeSlot::Enumerate(void* scope, ScopeSlot* slot) {
  Scope* scope_ = reinterpret_cast<Scope*>(scope);

  // Do not double process slots
  if (slot->index() != 0) return;

  if (slot->is_stack()) {
    slot->index(scope_->stack_index_++);
  } else if (slot->is_context()) {
    if (slot->depth() == 0) {
      slot->index(scope_->context_index_++);
      List<ScopeSlot*, ZoneObject>::Item* item = slot->uses()->head();
      while (item != NULL) {
        item->value()->index(slot->index());
        item = item->next();
      }
    }
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
    fn->variable(Visit(fn->variable()));
  }

  // Call takes variables from outer scope
  if (fn->children()->length() == 0) {
    AstList::Item* item = fn->args()->head();
    while (item != NULL) {
      item->value(Visit(item->value()));
      item = item->next();
    }
  }

  Scope scope(this, node->is_root() ? Scope::kBlock : Scope::kFunction);

  // Put variables in functions scope
  if (fn->children()->length() != 0) {
    AstList::Item* item = fn->args()->head();
    while (item != NULL) {
      item->value(Visit(item->value()));
      item = item->next();
    }
  }

  VisitChildren(node);

  node->SetScope(&scope);

  return node;
}


AstNode* ScopeAnalyze::VisitCall(AstNode* node) {
  return VisitFunction(node);
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
