#ifndef _SRC_ROOT_H_
#define _SRC_ROOT_H_

#include "utils.h" // List
#include "zone.h" // ZoneObject

namespace candor {
namespace internal {

// Forward declarations
class ScopeSlot;
class AstNode;
class Heap;
class HContext;

class Root {
 public:
  typedef ZoneList<char*> HValueList;

  Root(Heap* heap);

  ScopeSlot* Put(AstNode* node);
  HContext* Allocate();

  char* NumberToValue(AstNode* node, ScopeSlot* slot);
  char* StringToValue(AstNode* node);

  inline Heap* heap() { return heap_; }
  inline HValueList* values() { return &values_; }

 private:
  Heap* heap_;
  HValueList values_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_ROOT_H_
