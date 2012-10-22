#include "visitor.h"
#include "ast.h"
#include "hir.h" // HIRInstruction
#include "utils.h" // List
#include "zone.h" // ZoneObject

namespace candor {
namespace internal {

#define VISITOR_SWITCH(V, _) \
    case AstNode::k##V: \
      return Visit##V(node);

#define VISITOR_BLOCK_STUB(V, _) \
    template <class T> \
    T* Visitor<T>::Visit##V(AstNode* node) { \
      VisitChildren(node); \
      return NULL; \
    }

#define VISITOR_REGULAR_STUB(V, _) \
    template <class T> \
    T* Visitor<T>::Visit##V(AstNode* node) { \
      return NULL; \
    }

// Instantiate Visitor
template class Visitor<AstNode>;
template class Visitor<HIRInstruction>;

template <class T>
T* Visitor<T>::Visit(AstNode* node) {
  current_node_ = node;

  switch (node->type()) {
   VISITOR_MAPPING_BLOCK(VISITOR_SWITCH, 0)
   VISITOR_MAPPING_REGULAR(VISITOR_SWITCH, 0)
   default:
    VisitChildren(node);
    return NULL;
  }
}


template <class T>
void Visitor<T>::VisitChildren(AstNode* node) {
  ZoneList<AstList::Item*> blocks_queue;

  AstList::Item* child = node->children()->head();
  for (; child != NULL; child = child->next()) {
    // In breadth-first visiting
    // do not increase depth until all same-level nodes will be visited
    if (type_ == kBreadthFirst &&
        child->value()->is(AstNode::kFunction)) {
      blocks_queue.Push(child);
    } else {
      Visit(child->value());
    }
  }

  while ((child = blocks_queue.Shift()) != NULL) {
    Visit(child->value());
  }
}

VISITOR_MAPPING_BLOCK(VISITOR_BLOCK_STUB, 0)
VISITOR_MAPPING_REGULAR(VISITOR_REGULAR_STUB, 0)

} // namespace internal
} // namescape candor
