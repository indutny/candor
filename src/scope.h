#ifndef _SRC_SCOPE_H_
#define _SRC_SCOPE_H_

#include "parser.h"
#include "utils.h" // HashMap

namespace dotlang {

enum ScopeSlotType {
  kLocal,
  kScope
};

class ScopeSlot {
 public:
};

class Scope : public HashMap<ScopeSlot*> {
 public:
  Scope(Parser* parser) : parser_(parser) {
    parent_ = parser_->scope_;
    parser_->scope_ = this;
  }
  ~Scope() {
    parser_->scope_ = parent_;
  }

  ScopeSlot* GetSlot(const char* name,
                     uint32_t length,
                     ScopeSlotType type);

 private:
  Parser* parser_;
  Scope* parent_;
};

} // namespace dotlang

#endif // _SRC_SCOPE_H_
