#ifndef _SRC_FULLGEN_H_
#define _SRC_FULLGEN_H_

#include "visitor.h"

#if __ARCH == x64
#include "x64/macroassembler-x64.h"
#else
#include "ia32/macroassembler-ia32.h"
#endif

namespace dotlang {

class Fullgen : public Masm, public Visitor {
 public:
  Fullgen() : Masm(), Visitor(kPreorder) {
  }

  void GeneratePrologue(AstNode* stmt);
  void GenerateEpilogue(AstNode* stmt);
  void GenerateLookup(AstNode* name);

  AstNode* VisitFunction(AstNode* stmt);
  AstNode* VisitAssign(AstNode* stmt);
  AstNode* VisitValue(AstNode* node);
};

} // namespace dotlang

#endif // _SRC_FULLGEN_H_
