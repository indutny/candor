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

// Each AstVariable gets it's slot
// After parse end indexes will be allocated
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

  inline bool is_stack() { return type_ == kStack; }
  inline bool is_context() { return type_ == kContext; }

  inline int32_t index() { return index_; }
  inline void index(int32_t index) { index_ = index; }
  inline int32_t depth() { return depth_; }

  inline List<ScopeSlot*, ZoneObject>* uses() { return &uses_; }

  Type type_;
  int32_t index_;
  int32_t depth_;

  List<ScopeSlot*, ZoneObject> uses_;
};

// On each block or function enter new scope is created
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

// Runs on complete AST tree to wrap each kName into AstValue
// and put it either in stack or in some context
class ScopeAnalyze : public Visitor {
 public:
  ScopeAnalyze(AstNode* ast);

  AstNode* VisitFunction(AstNode* node);
  AstNode* VisitCall(AstNode* node);
  AstNode* VisitBlock(AstNode* node);
  AstNode* VisitScopeDecl(AstNode* node);
  AstNode* VisitName(AstNode* node);

  inline Scope* scope() { return scope_; }

 protected:
  AstNode* ast_;
  Scope* scope_;

  friend class Scope;
};

} // namespace dotlang

#endif // _SRC_SCOPE_H_
