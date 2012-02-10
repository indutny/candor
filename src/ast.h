#ifndef _SRC_AST_H_
#define _SRC_AST_H_

#include "utils.h" // List
#include "lexer.h" // lexer

namespace dotlang {

#define TYPE_MAPPING(V)\
    V(kName)\
    V(kNumber)\
    V(kString)\
    V(kTrue)\
    V(kFalse)\
    V(kNil)\
    V(kScope)\
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
class AstNode {
 public:
  enum Type {
    kBlock,
    kBlockExpr,
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

  AstNode(Type type) : type_(type) {
    value_ = NULL;
    length_ = 0;
    children_.allocated = true;
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

  inline AstNode* FromToken(Lexer::Token* tok) {
    value_ = tok->value_;
    length_ = tok->length_;

    return this;
  }

  inline List<AstNode*>* children() {
    return &children_;
  }

  inline Type type() {
    return type_;
  }

  inline bool is(Type type) {
    return type_ == type;
  }


  Type type_;
  const char* value_;
  uint32_t length_;

  List<AstNode*> children_;
};
#undef TYPE_MAPPING


class BlockStmt : public AstNode {
 public:
  BlockStmt(AstNode* scope) : AstNode(kBlock), scope_(scope) {
  }
  ~BlockStmt() {
    delete scope_;
  }

  AstNode* scope_;
};


class FunctionLiteral : public AstNode {
 public:
  FunctionLiteral(AstNode* variable, uint32_t offset) : AstNode(kFunction) {
    variable_ = variable;
    body_ = NULL;
    args_.allocated = true;

    offset_ = offset;
    length_ = 0;
  }

  ~FunctionLiteral() {
    delete variable_;
    delete body_;
  }

  inline bool CheckDeclaration() {
    // Function without body is a call
    if (body_ == NULL) {
      // So it should have a name
      if (variable_ == NULL) return false;
      return true;
    }

    // Name should not be "a.b.c"
    if (variable_ != NULL && !variable_->is(kName)) return false;

    // Arguments should be a kName, not expressions
    List<AstNode*>::Item* head;
    for (head = args_.Head(); head != NULL; head = args_.Next(head)) {
      if (!head->value->is(kName)) return false;
    }

    // Body should be a block
    if (!body_->is(kBlock)) return false;

    return true;
  }

  inline FunctionLiteral* End(uint32_t end) {
    length_ = end - offset_;
    return this;
  }

  inline List<AstNode*>* args() {
    return &args_;
  }

  AstNode* variable_;
  List<AstNode*> args_;
  AstNode* body_;

  uint32_t offset_;
  uint32_t length_;
};


class IfStmt : public AstNode {
 public:
  IfStmt(AstNode* condition, AstNode* body, AstNode* else_body) : AstNode(kIf) {
    cond_ = condition;
    body_ = body;
    else_ = else_body;
  }

  ~IfStmt() {
    delete cond_;
    delete body_;
    delete else_;
  }

  AstNode* cond_;
  AstNode* body_;
  AstNode* else_;
};


class WhileStmt : public AstNode {
 public:
  WhileStmt(AstNode* condition, AstNode* body) : AstNode(kWhile) {
    cond_ = condition;
    body_ = body;
  }

  ~WhileStmt() {
    delete cond_;
    delete body_;
  }

  AstNode* cond_;
  AstNode* body_;
};


class AssignExpr : public AstNode {
 public:
  AssignExpr(AstNode* variable, AstNode* value) : AstNode(kAssign) {
    variable_ = variable;
    value_ = value;
  }

  ~AssignExpr() {
    delete variable_;
    delete value_;
  }

  AstNode* variable_;
  AstNode* value_;
};


} // namespace dotlang

#endif // _SRC_AST_H_
