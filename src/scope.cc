#include "scope.h"
#include "ast.h" // AstNode, AstList

#include <assert.h>

namespace candor {
namespace internal {

Scope::Scope(ScopeAnalyze* a, Type type) : a_(a),
                                           type_(type),
                                           parent_(a->scope_) {
  a_->scope_ = this;

  stack_count_ = 0;
  context_count_ = 0;

  if (parent() != NULL && type_ != kFunction) {
    stack_index_ = parent()->stack_index_;
    context_index_ = parent()->context_index_;
  } else {
    stack_index_ = 0;
    context_index_ = 0;
  }

  // Increase depth when entering function
  // depth_ = 0 is for global object
  depth_ = parent() == NULL ? 1 : parent()->depth_;
  if (type_ == kFunction) depth_++;
}


Scope::~Scope() {
  assert(stack_count_ >= 0);
  assert(context_count_ >= 0);

  // Assign indexes to each stack item
  Enumerate(ScopeSlot::Enumerate);

  // Lift up stack and context sizes
  if (type_ != kFunction && parent() != NULL) {
    parent()->stack_count_ += stack_count_;
    parent()->context_count_ += context_count_;

    parent()->stack_index_ = stack_index_;
    parent()->context_index_ = context_index_;
  }

  a_->scope_ = parent();
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


AstValue* Scope::MoveToContext(AstNode* name) {
  int32_t depth = type() == kFunction ? 1 : 0;
  Scope* scope = parent();

  ScopeSlot* slot = NULL;
  while (scope != NULL) {
    slot = scope->Get(name->value(), name->length());
    if (slot != NULL) {
      if (slot->is_stack()) {
        slot->type(ScopeSlot::kContext);

        scope->stack_count_--;
        scope->context_count_++;
      }
      break;
    }

    if (scope->type() == kFunction) depth++;
    scope = scope->parent();
  }

  if (slot == NULL) {
    // No matching scope was found - allocate in global
    slot = new ScopeSlot(ScopeSlot::kContext, -1);
  } else {
    // Slot was found, create new one and link to it
    ScopeSlot* source = slot;
    slot = new ScopeSlot(ScopeSlot::kContext, depth);
    source->uses()->Push(slot);
  }
  return new AstValue(slot, name);
}


void ScopeSlot::Enumerate(void* scope, ScopeSlot* slot) {
  Scope* scope_ = reinterpret_cast<Scope*>(scope);

  // Do not double process slots
  if (slot->index() != -1) return;

  if (slot->is_stack()) {
    slot->index(scope_->stack_index_++);
  } else if (slot->is_context()) {
    if (slot->depth() <= 0) {
      List<ScopeSlot*, ZoneObject>::Item* item = slot->uses()->head();

      // Find if we've already indexed some of uses
      while (item != NULL) {
        if (item->value()->index() != -1) {
          slot->index(item->value()->index());
          break;
        }
        item = item->next();
      }

      if (slot->index() == -1) {
        slot->index(slot->depth() >= 0 ? scope_->context_index_++ : 0);
      }

      item = slot->uses()->head();
      while (item != NULL) {
        item->value()->index(slot->index());
        item = item->next();
      }
    }
  } else {
    UNEXPECTED
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


AstNode* ScopeAnalyze::VisitAt(AstNode* node) {
  AstNode* name = node->lhs();
  assert(name != NULL);

  return scope()->MoveToContext(name);
}


AstNode* ScopeAnalyze::VisitName(AstNode* node) {
  return new AstValue(scope(), node);
}

} // namespace internal
} // namespace candor
