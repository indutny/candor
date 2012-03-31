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
  if (parent() == NULL) {
    depth_ = 0;
    // And put a global object in
    Set(new StringKey<ZoneObject>("global", 6),
        new ScopeSlot(ScopeSlot::kContext, -1));
  } else {
    depth_ = parent()->depth_;
    if (type_ == kFunction) depth_++;
  }
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


ScopeSlot* Scope::GetSlot(const char* name, uint32_t length) {
  StringKey<ZoneObject>* key = new StringKey<ZoneObject>(name, length);
  ScopeSlot* slot = Get(key);

  if (slot != NULL) {
    slot->use();
    return slot;
  }

  // No slot was found - go through scopes up to the root one
  int depth = 1;
  Scope* scope = parent();
  while (scope != NULL) {
    slot = scope->Get(key);

    if (slot != NULL && slot->depth() <= 0) {
      if (slot->is_stack()) {
        slot->type(ScopeSlot::kContext);

        scope->stack_count_--;
        scope->context_count_++;
      } else if (slot->is_context() && slot->depth() == -1) {
        depth = -1;
      }
      break;
    }

    depth++;
    scope = scope->parent();
  }

  ScopeSlot* source = slot;

  if (source == NULL) {
    // Stack variable
    slot = new ScopeSlot(ScopeSlot::kStack);
    stack_count_++;
  } else {
    // Context variable
    slot = new ScopeSlot(ScopeSlot::kContext, depth);
    source->uses()->Push(slot);
    source->use();
  }
  slot->use();
  Set(key, slot);

  return slot;
}


void ScopeSlot::Enumerate(void* scope, ScopeSlot* slot) {
  Scope* scope_ = reinterpret_cast<Scope*>(scope);

  // Do not double process slots
  if (slot->index() != -1) return;

  if (slot->is_stack()) {
    slot->index(scope_->stack_index_++);
  } else if (slot->is_context()) {
    if (slot->depth() <= 0) {
      UseList::Item* item = slot->uses()->head();

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


void ScopeSlot::Print(PrintBuffer* p) {
  if (is_stack()) {
    p->Print("[st:%d]", index());
  } else if (is_context()) {
    p->Print("[ctx %d:%d]", depth(), index());
  } else if (is_immediate()) {
    p->Print("[imm %p]", value());
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
    if (fn->children()->length() != 0) {
      AstNode* assign = new AstNode(AstNode::kAssign);
      assign->children()->Push(fn->variable());
      assign->children()->Push(fn);
      fn->variable(NULL);
      return Visit(assign);
    } else {
      fn->variable(Visit(fn->variable()));
    }
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


AstNode* ScopeAnalyze::VisitName(AstNode* node) {
  return new AstValue(scope(), node);
}

} // namespace internal
} // namespace candor
