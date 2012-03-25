#ifndef _SRC_SCOPE_H_
#define _SRC_SCOPE_H_

#include "utils.h" // HashMap
#include "zone.h" // HashMap
#include "visitor.h" // Visitor

#include <assert.h> // assert

namespace candor {
namespace internal {

// Forward declarations
class AstNode;
class AstValue;
class Scope;
class ScopeAnalyze;

// Each AstVariable gets it's slot
// After parse end indexes will be allocated
class ScopeSlot : public ZoneObject {
 public:
  typedef List<ScopeSlot*, ZoneObject> UseList;
  enum Type {
    kStack,
    kContext
  };

  ScopeSlot(Type type) : type_(type), index_(-1), depth_(0), use_count_(0) {
  }

  ScopeSlot(Type type, int32_t depth) : type_(type),
                                        index_(depth < 0 ? 0 : -1),
                                        depth_(depth),
                                        use_count_(0) {
  }

  static void Enumerate(void* scope, ScopeSlot* slot);

  inline bool is_stack() { return type_ == kStack; }
  inline bool is_context() { return type_ == kContext; }

  inline void type(Type type) { type_ = type; }

  inline int32_t index() { return index_; }
  inline void index(int32_t index) { index_ = index; }
  inline int32_t depth() { return depth_; }
  inline void depth(int32_t depth) { depth_ = depth; }

  inline void use() { use_count_++; }
  inline int use_count() { return use_count_; }

  inline UseList* uses() { return &uses_; }

 private:
  Type type_;
  int32_t index_;
  int32_t depth_;
  int use_count_;

  UseList uses_;
};

// On each block or function enter new scope is created
class Scope : public HashMap<StringKey<ZoneObject>, ScopeSlot, ZoneObject> {
 public:
  enum Type {
    kBlock,
    kFunction
  };

  static void Analyze(AstNode* ast);

  Scope(ScopeAnalyze* a, Type type);
  ~Scope();

  ScopeSlot* GetSlot(const char* name, uint32_t length);

  inline int32_t stack_count() { return stack_count_; }
  inline int32_t context_count() { return context_count_; }
  inline Scope* parent() { return parent_; }
  inline Type type() { return type_; }

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
  AstNode* VisitName(AstNode* node);

  inline Scope* scope() { return scope_; }

 protected:
  AstNode* ast_;
  Scope* scope_;

  friend class Scope;
};

} // namespace internal
} // namespace candor

#endif // _SRC_SCOPE_H_
