#include "visitor.h"
#include "ast.h"
#include "utils.h" // List
#include "zone.h" // ZoneObject

namespace dotlang {

AstNode* Visitor::Visit(AstNode* node) {
  switch (node->type()) {
   case AstNode::kFunction:
    return VisitFunction(node);
   case AstNode::kBlock:
    return VisitBlock(node);
   case AstNode::kScopeDecl:
    return VisitScopeDecl(node);
   case AstNode::kAssign:
    return VisitAssign(node);
   case AstNode::kMember:
    return VisitMember(node);
   case AstNode::kName:
    return VisitName(node);
   case AstNode::kValue:
    return VisitValue(node);
   case AstNode::kNumber:
    return VisitNumber(node);
   default:
    VisitChildren(node);
    return node;
  }
}

void Visitor::VisitChildren(AstNode* node) {
  List<AstList::Item*, ZoneObject> blocks_queue;

  AstList::Item* child = node->children()->head();
  while (child != NULL) {
    // In breadth-first visiting
    // do not increase depth until all same-level nodes will be visited
    if (type_ == kBreadthFirst &&
        (child->value()->is(AstNode::kFunction) ||
        child->value()->is(AstNode::kBlock))) {
      blocks_queue.Push(child);
    } else {
      child->value(Visit(child->value()));
      child = child->next();
    }
  }

  while (blocks_queue.length() > 0) {
    child = blocks_queue.Shift();
    child->value(Visit(child->value()));
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


AstNode* Visitor::VisitAssign(AstNode* node) {
  VisitChildren(node);
  return node;
}


AstNode* Visitor::VisitMember(AstNode* node) {
  VisitChildren(node);
  return node;
}


AstNode* Visitor::VisitName(AstNode* node) {
  return node;
}


AstNode* Visitor::VisitValue(AstNode* node) {
  return node;
}


AstNode* Visitor::VisitNumber(AstNode* node) {
  return node;
}

} // namescape dotlang
