#ifndef _SRC_SCOPE_H_
#define _SRC_SCOPE_H_

#include "utils.h" // HashMap
#include "zone.h" // HashMap

namespace dotlang {

// Forward declarations
class Scope;
class Parser;

class ScopeSlot : public ZoneObject {
 public:
  enum Type {
    kStack,
    kContext
  };

  ScopeSlot(Type type) : type_(type), index_(0) {
  }

  static void Enumerate(void* scope, ScopeSlot* slot);

  inline bool isStack() { return type_ == kStack; }
  inline bool isContext() { return type_ == kContext; }
  inline uint32_t index() { return index_; }

  Type type_;
  uint32_t index_;
};

class Scope : public HashMap<ScopeSlot*, ZoneObject> {
 public:
  enum Type {
    kBlock,
    kFunction
  };

  Scope(Parser* parser, Type type);
  ~Scope();

  ScopeSlot* GetSlot(const char* name, uint32_t length);
  void MoveToContext(const char* name, uint32_t length);

  inline int32_t stack_count() {
    return stack_count_;
  }

  inline int32_t context_count() {
    return context_count_;
  }

 protected:
  int32_t stack_count_;
  int32_t context_count_;

  int32_t stack_index_;
  int32_t context_index_;

  Parser* parser_;
  Type type_;

  Scope* parent_;

  friend class ScopeSlot;
};

} // namespace dotlang

#endif // _SRC_SCOPE_H_
