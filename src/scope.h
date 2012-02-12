#ifndef _SRC_SCOPE_H_
#define _SRC_SCOPE_H_

#include "utils.h" // HashMap
#include "zone.h" // HashMap
#include "visitor.h" // Visitor

#include <assert.h> // assert

namespace dotlang {

// Forward declarations
class AstNode;
class Scope;
class ScopeAnalyze;

class ScopeSlot : public ZoneObject {
 public:
  enum Type {
    kStack,
    kContext
  };

  ScopeSlot(Type type) : type_(type), index_(0), depth_(0) {
  }

  ScopeSlot(Type type, uint32_t depth) : type_(type), index_(0), depth_(depth) {
  }

  static void Enumerate(void* scope, ScopeSlot* slot);

  inline bool isStack() { return type_ == kStack; }
  inline bool isContext() { return type_ == kContext; }
  inline uint32_t index() { return index_; }
  inline uint32_t depth() { return depth_; }

  Type type_;
  uint32_t index_;
  uint32_t depth_;
};

class Scope : public HashMap<ScopeSlot*, ZoneObject> {
 public:
  enum Type {
    kBlock,
    kFunction
  };

  static void Analyze(AstNode* ast);

  Scope(ScopeAnalyze* a, Type type);
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

  int32_t depth_;

  ScopeAnalyze* a_;
  Type type_;

  Scope* parent_;

  friend class ScopeSlot;
};

class ScopeAnalyze : public Visitor {
 public:
  ScopeAnalyze(AstNode* ast);

  AstNode* VisitFunction(AstNode* node);
  AstNode* VisitBlock(AstNode* node);
  AstNode* VisitScopeDecl(AstNode* node);
  AstNode* VisitName(AstNode* node);

 protected:
  Scope* scope_;

  friend class Scope;
};

} // namespace dotlang

#endif // _SRC_SCOPE_H_
