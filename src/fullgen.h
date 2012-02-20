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

class Fullgen : public Masm, public Visitor {
 public:
  Fullgen(Heap* heap) : Masm(heap), Visitor(kPreorder), heap_(heap) {
  }

  void GeneratePrologue(AstNode* stmt);
  void GenerateEpilogue(AstNode* stmt);
  void GenerateLookup(AstNode* name);

  AstNode* VisitFunction(AstNode* stmt);
  AstNode* VisitAssign(AstNode* stmt);
  AstNode* VisitValue(AstNode* node);
  AstNode* VisitNumber(AstNode* node);

  void VisitForValue(AstNode* node, Register reg);

  inline Heap* heap() { return heap_; }

 private:
  Heap* heap_;
};

} // namespace dotlang

#endif // _SRC_FULLGEN_H_
