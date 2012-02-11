#include "scope.h"
#include "parser.h"

#include <assert.h>

namespace dotlang {

Scope::Scope(Parser* parser, Type type) : parser_(parser), type_(type) {
  parent_ = parser_->scope_;
  parser_->scope_ = this;

  stack_count_ = 0;
  context_count_ = 0;

  if (parent_ != NULL && type_ != kFunction) {
    stack_index_ = parent_->stack_index_;
    context_index_ = parent_->context_index_;
  } else {
    stack_index_ = 0;
    context_index_ = 0;
  }
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

  parser_->scope_ = parent_;
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
    {
      // Bubble up until parent will be found
      Scope* scope = parent_;
      while (scope != NULL) {
        if (scope->Get(name, length) != NULL)  {
          parent_->MoveToContext(name, length);
          break;
        }

        scope = scope->parent_;
      }
    }

    slot = new ScopeSlot(ScopeSlot::kContext);
    Set(name, length, slot);
  } else if (slot->isStack()) {
    // Variable was stored in stack, but should be moved into context
    slot->type_ = ScopeSlot::kContext;
    stack_count_--;
    context_count_++;
  }
}


void ScopeSlot::Enumerate(void* scope, ScopeSlot* slot) {
  if (slot->isStack()) {
    slot->index_ = reinterpret_cast<Scope*>(scope)->stack_index_++;
  } else if (slot->isContext()) {
    slot->index_ = reinterpret_cast<Scope*>(scope)->context_index_++;
  } else {
    assert(0 && "Unreachable");
  }
}

} // namespace dotlang
