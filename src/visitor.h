#ifndef _SRC_VISITOR_H_
#define _SRC_VISITOR_H_

namespace dotlang {

// Forward declaration
class AstNode;

class Visitor {
 public:
  AstNode* Visit(AstNode* node);
  void VisitChildren(AstNode* node);

  virtual AstNode* VisitFunction(AstNode* node);
  virtual AstNode* VisitBlock(AstNode* node);
  virtual AstNode* VisitScopeDecl(AstNode* node);
  virtual AstNode* VisitName(AstNode* node);
};

} // namespace dotlang

#endif // _SRC_VISITOR_H_
