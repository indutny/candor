#ifndef _SRC_FULLGEN_H_
#define _SRC_FULLGEN_H_

#include "visitor.h"

#if __ARCH == x64
#include "x64/macroassembler-x64.h"
#else
#include "ia32/macroassembler-ia32.h"
#endif

namespace dotlang {

// Forward declaration
class Heap;
class Register;

// Generates non-optimized code by visiting each node in AST tree in-order
class Fullgen : public Masm, public Visitor {
 public:
  enum VisitorType {
    kValue,
    kSlot
  };

  Fullgen(Heap* heap) : Masm(heap),
                        Visitor(kPreorder),
                        heap_(heap),
                        visitor_type_(kValue) {
  }

  void GeneratePrologue(AstNode* stmt);
  void GenerateEpilogue(AstNode* stmt);
  void GenerateLookup(AstNode* name);

  AstNode* VisitFunction(AstNode* stmt);
  AstNode* VisitAssign(AstNode* stmt);
  AstNode* VisitValue(AstNode* node);
  AstNode* VisitNumber(AstNode* node);

  AstNode* VisitForValue(AstNode* node, Register reg);
  AstNode* VisitForSlot(AstNode* node, Operand* op, Register base);

  inline Heap* heap() { return heap_; }
  inline bool visiting_for_value() { return visitor_type_ == kValue; }
  inline bool visiting_for_slot() { return visitor_type_ == kSlot; }

 private:
  Heap* heap_;
  VisitorType visitor_type_;
};

} // namespace dotlang

#endif // _SRC_FULLGEN_H_
