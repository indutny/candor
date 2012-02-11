#include "assembler-x64.h"
#include "fullgen.h"
#include "ast.h" // AstNode
#include "utils.h" // List

#include <assert.h>

namespace dotlang {

void Fullgen::GeneratePrologue() {
  push(rbp);
  movq(rbp, rsp);
}


void Fullgen::GenerateEpilogue() {
  movq(rsp, rbp);
  pop(rbp);
  ret(0);
}


void Fullgen::GenerateFunction(AstNode* stmt) {
  GeneratePrologue();
  GenerateEpilogue();
}


void Fullgen::GenerateBlock(AstNode* stmt) {
  List<AstNode*>::Item* i;
  for (i = stmt->children()->Head(); i != NULL; i = stmt->children()->Next(i)) {
    switch (i->value()->type()) {
     case AstNode::kAssign:
      GenerateAssign(i->value());
      break;
     default:
      assert(0 && "not implemented");
      break;
    }
  }
}


void Fullgen::GenerateAssign(AstNode* stmt) {
}


void Fullgen::GenerateStackLookup(AstNode* name) {
}


void Fullgen::GenerateScopeLookup(AstNode* name) {
}


} // namespace dotlang
