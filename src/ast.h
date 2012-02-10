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

  AstNode(Type type_) : type(type_) {
    value = NULL;
    length = 0;
    children.allocated = true;
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

  AstNode* FromToken(Lexer::Token* tok) {
    value = tok->value;
    length = tok->length;

    return this;
  }


  Type type;
  const char* value;
  uint32_t length;

  List<AstNode*> children;
};
#undef TYPE_MAPPING


class BlockStmt : public AstNode {
 public:
  BlockStmt(AstNode* scope_) : AstNode(kBlock), scope(scope_) {
  }
  ~BlockStmt() {
    delete scope;
  }

  AstNode* scope;
};


class FunctionLiteral : public AstNode {
 public:
  FunctionLiteral(AstNode* variable_) : AstNode(kFunction) {
    variable = variable_;
    body = NULL;
    args.allocated = true;
  }

  ~FunctionLiteral() {
    delete variable;
    delete body;
  }

  bool CheckDeclaration() {
    // Function without body is a call
    if (body == NULL) {
      // So it should have a name
      if (variable == NULL) return false;
      return true;
    }

    // Name should not be "a.b.c"
    if (variable != NULL && variable->type != kName) return false;

    // Arguments should be a kName, not expressions
    List<AstNode*>::Item* head = args.Head();
    while (head != NULL) {
      if (head->value->type != kName) return false;
      head = args.Next(head);
    }

    // Body should be a block
    if (body->type != kBlock) return false;

    return true;
  }

  AstNode* variable;
  List<AstNode*> args;
  AstNode* body;
};


class IfStmt : public AstNode {
 public:
  IfStmt(AstNode* condition, AstNode* body_, AstNode* else_) : AstNode(kIf) {
    cond = condition;
    body = body_;
    else_body = else_;
  }

  ~IfStmt() {
    delete cond;
    delete body;
    delete else_body;
  }

  AstNode* cond;
  AstNode* body;
  AstNode* else_body;
};


class WhileStmt : public AstNode {
 public:
  WhileStmt(AstNode* condition, AstNode* body_) : AstNode(kWhile) {
    cond = condition;
    body = body_;
  }

  ~WhileStmt() {
    delete cond;
    delete body;
  }

  AstNode* cond;
  AstNode* body;
};


class AssignExpr : public AstNode {
 public:
  AssignExpr(AstNode* variable_, AstNode* value_) : AstNode(kAssign) {
    variable = variable_;
    value = value_;
  }

  ~AssignExpr() {
    delete variable;
    delete value;
  }

  AstNode* variable;
  AstNode* value;
};


} // namespace dotlang

#endif // _SRC_AST_H_
