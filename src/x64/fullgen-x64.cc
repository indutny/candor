#include "macroassembler-x64.h"
#include "fullgen.h"
#include "ast.h" // AstNode
#include "utils.h" // List

#include <assert.h>
#include <stdint.h>

namespace dotlang {

void Fullgen::GeneratePrologue(AstNode* stmt) {
  push(rbp);
  movq(rbp, rsp);

  // Allocate space for on stack variables
  subq(rsp, Immediate(RoundUp((stmt->stack_slots() + 1) * sizeof(void*), 16)));
}


void Fullgen::GenerateEpilogue(AstNode* stmt) {
  movq(rsp, rbp);
  pop(rbp);
  ret(0);
}


void Fullgen::GenerateLookup(AstNode* name) {
}


AstNode* Fullgen::VisitFunction(AstNode* stmt) {
  GeneratePrologue(stmt);
  VisitChildren(stmt);
  GenerateEpilogue(stmt);

  return stmt;
}


AstNode* Fullgen::VisitAssign(AstNode* stmt) {
  AstNode* lhs = Visit(stmt->children()->head()->value());
  AstNode* rhs = Visit(stmt->children()->head()->next()->value());

  Mov(MValue::Cast(lhs), MValue::Cast(rhs));

  return stmt;
}


AstNode* Fullgen::VisitValue(AstNode* node) {
  return new MValue(AstValue::Cast(node));
}


} // namespace dotlang
