#ifndef _SRC_AST_H_
#define _SRC_AST_H_

#include "zone.h" // ZoneObject
#include "utils.h" // List
#include "lexer.h" // lexer
#include "scope.h" // Scope

namespace dotlang {

// Forward declaration
struct ScopeSlot;
class AstNode;

typedef List<AstNode*, ZoneObject> AstList;

#define TYPE_MAPPING(V)\
    V(kName)\
    V(kNumber)\
    V(kString)\
    V(kTrue)\
    V(kFalse)\
    V(kNil)\
    V(kAdd)\
    V(kSub)\
    V(kDiv)\
    V(kMul)\
    V(kBAnd)\
    V(kBOr)\
    V(kBXor)\
    V(kEq)\
    V(kStrictEq)\
    V(kNe)\
    V(kStrictNe)\
    V(kLt)\
    V(kGt)\
    V(kLe)\
    V(kGe)\
    V(kLOr)\
    V(kLAnd)

// Base class
class AstNode : public ZoneObject {
 public:
  enum Type {
    kBlock,
    kBlockExpr,
    kScopeDecl,
    kMember,
    kAssign,
    kIf,
    kWhile,
    kBreak,
    kReturn,
    kFunction,

    // Prefixes
    kPreInc,
    kPreDec,
    kNot,

    // Postfixes,
    kPostInc,
    kPostDec,

    // Binop
#define MAP_DF(x) x,
    TYPE_MAPPING(MAP_DF)
#undef MAP_DF

    kNop
  };

  AstNode(Type type) : type_(type),
                       value_(NULL),
                       length_(0),
                       stack_count_(0),
                       context_count_(0) {
  }

  virtual ~AstNode() {
  }

  inline static Type ConvertType(Lexer::TokenType type) {
    switch (type) {
#define MAP_DF(x) case Lexer::x: return x;
      TYPE_MAPPING(MAP_DF)
#undef MAP_DF
     default:
      return kNop;
    }
  }

  inline AstNode* FromToken(Lexer::Token* token) {
    value_ = token->value_;
    length_ = token->length_;

    return this;
  }

  inline AstList* children() { return &children_; }
  inline Type type() { return type_; }
  inline bool is(Type type) { return type_ == type; }
  inline int32_t stack_slots() { return stack_count_; }
  inline int32_t context_slots() { return context_count_; }

  inline void SetScope(Scope* scope) {
    stack_count_ = scope->stack_count();
    context_count_ = scope->context_count();
  }

  Type type_;

  const char* value_;
  uint32_t length_;

  int32_t stack_count_;
  int32_t context_count_;

  AstList children_;
};
#undef TYPE_MAPPING


class FunctionLiteral : public AstNode {
 public:
  FunctionLiteral(AstNode* variable, uint32_t offset) : AstNode(kFunction) {
    variable_ = variable;

    offset_ = offset;
    length_ = 0;
  }

  inline bool CheckDeclaration() {
    // Function without body is a call
    if (children()->Length() == 0) {
      // So it should have a name
      if (variable_ == NULL) return false;
      return true;
    }

    // Name should not be "a.b.c"
    if (variable_ != NULL && !variable_->is(kName)) return false;

    // Arguments should be a kName, not expressions
    AstList::Item* head;
    for (head = args_.Head(); head != NULL; head = args_.Next(head)) {
      if (!head->value()->is(kName)) return false;
    }

    return true;
  }

  inline FunctionLiteral* End(uint32_t end) {
    length_ = end - offset_;
    return this;
  }

  inline AstList* args() { return &args_; }

  AstNode* variable_;
  AstList args_;

  uint32_t offset_;
  uint32_t length_;
};


class AstValue : public AstNode {
 public:
  AstValue(Scope* scope, AstNode* name) : AstNode(kName) {
    slot_ = scope->GetSlot(name->value_, name->length_);
    name_ = name;
  }

  inline ScopeSlot* slot() { return slot_; }
  inline AstNode* name() { return name_; }

 protected:
  ScopeSlot* slot_;
  AstNode* name_;
};

} // namespace dotlang

#endif // _SRC_AST_H_
