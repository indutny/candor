#include "visitor.h"
#include "ast.h"

namespace dotlang {

AstNode* Visitor::Visit(AstNode* node) {
  switch (node->type()) {
   case AstNode::kFunction:
    return VisitFunction(node);
   case AstNode::kBlock:
    return VisitBlock(node);
   case AstNode::kScopeDecl:
    return VisitScopeDecl(node);
   case AstNode::kName:
    return VisitName(node);
   default:
    VisitChildren(node);
    return node;
  }
}

void Visitor::VisitChildren(AstNode* node) {
  AstList::Item* child = node->children()->head();
  while (child != NULL) {
    child->value(Visit(child->value()));
    child = child->next();
  }
}


AstNode* Visitor::VisitFunction(AstNode* node) {
  VisitChildren(node);
  return node;
}


AstNode* Visitor::VisitBlock(AstNode* node) {
  VisitChildren(node);
  return node;
}


AstNode* Visitor::VisitScopeDecl(AstNode* node) {
  return node;
}


AstNode* Visitor::VisitName(AstNode* node) {
  return node;
}

} // namescape dotlang
