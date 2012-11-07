#include "root.h"
#include "scope.h" // ScopeSlot
#include "ast.h" // AstNode
#include "heap.h" // HContext
#include "heap-inl.h"
#include "utils.h" // List

namespace candor {
namespace internal {

Root::Root(Heap* heap) : heap_(heap) {
  // Create a `global` object
  values()->Push(HObject::NewEmpty(heap));

  // Place some root values
  values()->Push(HBoolean::New(heap, Heap::kTenureOld, true));
  values()->Push(HBoolean::New(heap, Heap::kTenureOld, false));

  // Place types
  values()->Push(heap->CreateString("nil", 3));
  values()->Push(heap->CreateString("boolean", 7));
  values()->Push(heap->CreateString("number", 6));
  values()->Push(heap->CreateString("string", 6));
  values()->Push(heap->CreateString("object", 6));
  values()->Push(heap->CreateString("array", 5));
  values()->Push(heap->CreateString("function", 8));
  values()->Push(heap->CreateString("cdata", 5));
}


ScopeSlot* Root::Put(AstNode* node) {
  ScopeSlot* slot = new ScopeSlot(ScopeSlot::kContext, -2);

  slot->index(values()->length());

  char* value = HNil::New();
  switch (node->type()) {
   case AstNode::kNumber:
    value = NumberToValue(node, slot);
    break;
   case AstNode::kProperty:
   case AstNode::kString:
    value = StringToValue(node);
    break;
   case AstNode::kTrue:
    value = HBoolean::New(heap(), Heap::kTenureOld, true);
    break;
   case AstNode::kFalse:
    value = HBoolean::New(heap(), Heap::kTenureOld, false);
    break;
   case AstNode::kNil:
    slot->type(ScopeSlot::kImmediate);
    slot->value(HNil::New());
    break;
   default: UNEXPECTED break;
  }
  if (value != NULL) {
    values()->Push(value);
  }

  return slot;
}


char* Root::NumberToValue(AstNode* node, ScopeSlot* slot) {
  if (StringIsDouble(node->value(), node->length())) {
    // Allocate boxed heap number
    double value = StringToDouble(node->value(), node->length());

    return HNumber::New(heap(), Heap::kTenureOld, value);
  } else {
    // Allocate unboxed number
    int64_t value = StringToInt(node->value(), node->length());

    // Change slot's type
    slot->type(ScopeSlot::kImmediate);
    slot->value(HNumber::New(heap(), value));

    return NULL;
  }
}


char* Root::StringToValue(AstNode* node) {
  uint32_t length;
  const char* unescaped = Unescape(node->value(), node->length(), &length);

  char* result = heap()->CreateString(unescaped, length);

  delete unescaped;

  return result;
}


HContext* Root::Allocate() {
  return HValue::As<HContext>(HContext::New(heap(), values()));
}

} // namespace internal
} // namespace candor
