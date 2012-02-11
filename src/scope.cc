#include "scope.h"
#include "parser.h"

namespace dotlang {

Scope::Scope(Parser* parser) {
  parser_ = parser;

  parent_ = parser_->scope_;
  parser_->scope_ = this;
}


Scope::~Scope() {
  parser_->scope_ = parent_;
}


ScopeSlot* Scope::GetSlot(const char* name,
                          uint32_t length) {
  ScopeSlot* slot = Get(name, length);

  if (slot == NULL) {
    slot = new ScopeSlot();
    slot->type_ = ScopeSlot::kStack;
    Set(name, length, slot);
  }

  return slot;
}

void Scope::MoveToContext(const char* name, uint32_t length) {
  ScopeSlot* slot = Get(name, length);

  if (slot == NULL) {
    slot = new ScopeSlot();
    Set(name, length, slot);
  }
  slot->type_ = ScopeSlot::kContext;
}

} // namespace dotlang
