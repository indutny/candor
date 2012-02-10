#include "assembler-x64.h"
#include "fullgen.h"

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


void Fullgen::GenerateBlock(BlockStmt* stmt) {
}


void Fullgen::GenerateAssign(AssignExpr* stmt) {
}


void Fullgen::GenerateStackLookup(AstNode* name) {
}


void Fullgen::GenerateScopeLookup(AstNode* name) {
}


} // namespace dotlang
