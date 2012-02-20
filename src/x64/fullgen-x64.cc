#include "macroassembler-x64.h"
#include "fullgen.h"
#include "heap.h" // Heap
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


void Fullgen::VisitForValue(AstNode* node, Register reg) {
  Register stored = result_;
  result_ = reg;

  Visit(node);

  result_ = stored;
}


AstNode* Fullgen::VisitFunction(AstNode* stmt) {
  GeneratePrologue(stmt);
  VisitChildren(stmt);
  GenerateEpilogue(stmt);

  return stmt;
}


AstNode* Fullgen::VisitAssign(AstNode* stmt) {
  emitb(0xcc);
  VisitForValue(stmt->rhs(), rbx);
  Operand rhs(rbx, 0);
  movq(rbx, rhs);

  push(rbx);
  VisitForValue(stmt->lhs(), rax);
  pop(rbx);

  Operand lhs(rax, 0);
  movq(lhs, rbx);

  return stmt;
}


AstNode* Fullgen::VisitValue(AstNode* node) {
  return new MValue(AstValue::Cast(node));
}


AstNode* Fullgen::VisitNumber(AstNode* node) {
  MValue* v = new MValue();
  v->reg(result());

  Label runtime_alloc, finish;

  Register result_end = result().is(rbx) ? rax : rbx;
  Allocate(result(), result_end, 4, scratch, &runtime_alloc);

  jmp(&finish);
  bind(&runtime_alloc);

  emitb(0xcc);

  bind(&finish);

  return v;
}


} // namespace dotlang
