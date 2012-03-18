#include "visitor.h"
#include "ast.h"
#include "utils.h" // List
#include "zone.h" // ZoneObject

namespace candor {
namespace internal {

#define VISITOR_MAPPING_BLOCK(V)\
    V(Function)\
    V(Call)\
    V(Block)\
    V(If)\
    V(While)\
    V(Assign)\
    V(Member)\
    V(ObjectLiteral)\
    V(ArrayLiteral)\
    V(Return)\
    V(Break)\
    V(Continue)\
    V(Typeof)\
    V(Sizeof)\
    V(Keysof)\
    V(UnOp)\
    V(BinOp)

#define VISITOR_MAPPING_REGULAR(V)\
    V(Name)\
    V(Value)\
    V(Number)\
    V(Property)\
    V(String)\
    V(Nil)\
    V(True)\
    V(False)

#define VISITOR_SWITCH(V)\
    case AstNode::k##V:\
      return Visit##V(node);

#define VISITOR_BLOCK_STUB(V)\
    AstNode* Visitor::Visit##V(AstNode* node) {\
      VisitChildren(node);\
      return node;\
    }

#define VISITOR_REGULAR_STUB(V)\
    AstNode* Visitor::Visit##V(AstNode* node) {\
      return node;\
    }

AstNode* Visitor::Visit(AstNode* node) {
  current_node_ = node;

  switch (node->type()) {
   VISITOR_MAPPING_BLOCK(VISITOR_SWITCH)
   VISITOR_MAPPING_REGULAR(VISITOR_SWITCH)
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
      child = child->next();
    } else {
      child->value(Visit(child->value()));
      child = child->next();
    }
  }

  while ((child = blocks_queue.Shift()) != NULL) {
    child->value(Visit(child->value()));
  }
}

VISITOR_MAPPING_BLOCK(VISITOR_BLOCK_STUB)
VISITOR_MAPPING_REGULAR(VISITOR_REGULAR_STUB)

} // namespace internal
} // namescape candor
