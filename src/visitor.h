#ifndef _SRC_VISITOR_H_
#define _SRC_VISITOR_H_

namespace candor {
namespace internal {

// Forward declaration
class AstNode;

// AST Visiting abstraction
class Visitor {
 public:
  enum Type {
    kPreorder,
    kBreadthFirst
  };

  Visitor(Type type) : type_(type) {
  }

  AstNode* Visit(AstNode* node);
  void VisitChildren(AstNode* node);

  virtual AstNode* VisitFunction(AstNode* node);
  virtual AstNode* VisitCall(AstNode* node);
  virtual AstNode* VisitBlock(AstNode* node);
  virtual AstNode* VisitScopeDecl(AstNode* node);
  virtual AstNode* VisitIf(AstNode* node);
  virtual AstNode* VisitWhile(AstNode* node);
  virtual AstNode* VisitAssign(AstNode* node);
  virtual AstNode* VisitMember(AstNode* node);
  virtual AstNode* VisitName(AstNode* node);
  virtual AstNode* VisitValue(AstNode* node);
  virtual AstNode* VisitNumber(AstNode* node);
  virtual AstNode* VisitObjectLiteral(AstNode* node);
  virtual AstNode* VisitArrayLiteral(AstNode* node);
  virtual AstNode* VisitNil(AstNode* node);
  virtual AstNode* VisitTrue(AstNode* node);
  virtual AstNode* VisitFalse(AstNode* node);
  virtual AstNode* VisitReturn(AstNode* node);
  virtual AstNode* VisitProperty(AstNode* node);
  virtual AstNode* VisitString(AstNode* node);

  virtual AstNode* VisitTypeof(AstNode* node);
  virtual AstNode* VisitSizeof(AstNode* node);
  virtual AstNode* VisitKeysof(AstNode* node);

  virtual AstNode* VisitUnOp(AstNode* node);
  virtual AstNode* VisitBinOp(AstNode* node);

 private:
  Type type_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_VISITOR_H_
