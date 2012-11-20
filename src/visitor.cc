/**
 * Copyright (c) 2012, Fedor Indutny.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "visitor.h"
#include "ast.h"
#include "hir.h"
#include "hir-instructions.h"  // HIRInstruction
#include "hir-instructions-inl.h"  // HIRInstruction
#include "fullgen.h"
#include "fullgen-instructions.h"  // FInstruction
#include "fullgen-instructions-inl.h"  // FInstruction
#include "utils.h"  // List
#include "zone.h"  // ZoneObject

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
template class Visitor<FInstruction>;

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
  ZoneList<AstList::Item*>* old = NULL;
  if (node->is(AstNode::kFunction)) {
    old = queue_;
    queue_ = &blocks_queue;
  }

  AstList::Item* child = node->children()->head();
  for (; child != NULL; child = child->next()) {
    // In breadth-first visiting
    // do not increase depth until all same-level nodes will be visited
    if (type_ == kBreadthFirst &&
        child->value()->is(AstNode::kFunction)) {
      queue_->Push(child);
    } else {
      Visit(child->value());
    }
  }

  while ((child = blocks_queue.Shift()) != NULL) {
    Visit(child->value());
  }

  if (node->is(AstNode::kFunction)) queue_ = old;
}

VISITOR_MAPPING_BLOCK(VISITOR_BLOCK_STUB, 0)
VISITOR_MAPPING_REGULAR(VISITOR_REGULAR_STUB, 0)

FunctionIterator::FunctionIterator(AstNode* root) : Visitor(kPreorder) {
  work_queue_.Push(root);
}


FunctionLiteral* FunctionIterator::Value() {
  return FunctionLiteral::Cast(work_queue_.head()->value());
}


bool FunctionIterator::IsEnded() {
  return work_queue_.length() == 0;
}


void FunctionIterator::Advance() {
  AstNode* node = work_queue_.Shift();
  VisitChildren(node);
}


AstNode* FunctionIterator::VisitCall(AstNode* node) {
  FunctionLiteral* fn = FunctionLiteral::Cast(node);

  Visit(fn->variable());

  AstList::Item* arg = fn->args()->head();
  for (; arg != NULL; arg = arg->next()) {
    Visit(arg->value());
  }

  return node;
}


AstNode* FunctionIterator::VisitFunction(AstNode* node) {
  work_queue_.Push(node);
  return node;
}

}  // namespace internal
}  // namescape candor
