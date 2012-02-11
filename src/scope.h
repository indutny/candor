#ifndef _SRC_SCOPE_H_
#define _SRC_SCOPE_H_

#include "utils.h" // HashMap
#include "zone.h" // HashMap

namespace dotlang {

class Parser;

class ScopeSlot : public ZoneObject {
 public:
  enum Type {
    kStack,
    kContext
  };

  ScopeSlot(Type type) : type_(type) {
  }

  Type type_;
};

class Scope : public HashMap<ScopeSlot*, ZoneObject> {
 public:
  Scope(Parser* parser);
  ~Scope();

  ScopeSlot* GetSlot(const char* name, uint32_t length);
  void MoveToContext(const char* name, uint32_t length);

 private:
  uint32_t stack_count_;
  uint32_t context_count_;

  Parser* parser_;
  Scope* parent_;
};

} // namespace dotlang

#endif // _SRC_SCOPE_H_
