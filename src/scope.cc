#include "scope.h"
#include "parser.h"

namespace dotlang {

Scope::Scope(Parser* parser) {
  parser_ = parser;

  parent_ = parser_->scope_;
  parser_->scope_ = this;

  stack_count_ = 0;
  context_count_ = 0;
}


Scope::~Scope() {
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
    slot = new ScopeSlot(ScopeSlot::kContext);
    Set(name, length, slot);
    context_count_++;
  }

  if (slot->type_ != ScopeSlot::kContext) {
    slot->type_ = ScopeSlot::kContext;
    stack_count_--;
    context_count_++;
  }
}

} // namespace dotlang
