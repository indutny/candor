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
  values()->Push(heap->CreateBoolean(true));
  values()->Push(heap->CreateBoolean(false));

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

ScopeSlot* Root::GetSlot(char* value) {
  ScopeSlot* slot = map_.Get(NumberKey::New(value));

  if (slot != NULL) return slot;
  slot = new ScopeSlot(ScopeSlot::kContext, -2);
  slot->index(values()->length());
  values()->Push(value);
  map_.Set(NumberKey::New(value), slot);

  return slot;
}


ScopeSlot* Root::Put(AstNode* node) {
  ScopeSlot* slot = NULL;
  char* value = HNil::New();

  switch (node->type()) {
   case AstNode::kNumber:
    value = NumberToValue(node, &slot);
    break;
   case AstNode::kProperty:
   case AstNode::kString:
    value = StringToValue(node);
    break;
   case AstNode::kTrue:
    value = heap()->CreateBoolean(true);
    break;
   case AstNode::kFalse:
    value = heap()->CreateBoolean(false);
    break;
   case AstNode::kNil:
    {
      slot = new ScopeSlot(ScopeSlot::kContext, -2);
      slot->type(ScopeSlot::kImmediate);
      slot->value(HNil::New());
    }
   default: UNEXPECTED break;
  }

  if (slot != NULL) return slot;

  assert(value != NULL);
  return GetSlot(value);
}


char* Root::NumberToValue(AstNode* node, ScopeSlot** slot) {
  if (StringIsDouble(node->value(), node->length())) {
    // Allocate boxed heap number
    double value = StringToDouble(node->value(), node->length());

    return heap()->CreateNumber(value);
  } else {
    // Allocate unboxed number
    int64_t value = StringToInt(node->value(), node->length());

    // Change slot's type
    ScopeSlot* s =  new ScopeSlot(ScopeSlot::kContext, -2);
    s->type(ScopeSlot::kImmediate);
    s->value(HNumber::New(heap(), value));
    *slot = s;

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
