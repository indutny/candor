#ifndef _SRC_FULLGEN_H_
#define _SRC_FULLGEN_H_

#include "ast.h"

#if __ARCH == x64
#include "x64/assembler-x64.h"
#else
#include "ia32/assembler-ia32.h"
#endif

namespace dotlang {

class Fullgen : public Assembler {
 public:
  Fullgen() : Assembler() {
  }

  void GeneratePrologue();
  void GenerateEpilogue();

  void GenerateFunction(AstNode* stmt);
  void GenerateBlock(AstNode* stmt);
  void GenerateAssign(AstNode* stmt);
  void GenerateStackLookup(AstNode* name);
  void GenerateScopeLookup(AstNode* name);
};

} // namespace dotlang

#endif // _SRC_FULLGEN_H_
