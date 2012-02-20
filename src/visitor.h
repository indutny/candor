#ifndef _SRC_VISITOR_H_
#define _SRC_VISITOR_H_

namespace dotlang {

// Forward declaration
class AstNode;

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
  virtual AstNode* VisitBlock(AstNode* node);
  virtual AstNode* VisitScopeDecl(AstNode* node);
  virtual AstNode* VisitAssign(AstNode* node);
  virtual AstNode* VisitMember(AstNode* node);
  virtual AstNode* VisitName(AstNode* node);
  virtual AstNode* VisitValue(AstNode* node);
  virtual AstNode* VisitNumber(AstNode* node);

 private:
  Type type_;
};

} // namespace dotlang

#endif // _SRC_VISITOR_H_
