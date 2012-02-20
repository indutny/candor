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
  push(rsi); // context
  push(rdi); // heap
  movq(rbp, rsp);

  // Allocate space for on stack variables
  subq(rsp, Immediate(RoundUp((stmt->stack_slots() + 1) * sizeof(void*), 16)));

  // Allocate context
  AllocateContext(stmt->context_slots());
}


void Fullgen::GenerateEpilogue(AstNode* stmt) {
  movq(rsp, rbp);
  pop(rdi); // restore heap
  pop(rsi); // restore context
  pop(rbp);
  ret(0);
}


AstNode* Fullgen::VisitForValue(AstNode* node, Register reg) {
  Register stored = result_;
  VisitorType stored_type = visitor_type_;

  result_ = reg;
  visitor_type_ = kValue;

  AstNode* result = Visit(node);

  result_ = stored;
  visitor_type_ = stored_type;

  return result;
}


AstNode* Fullgen::VisitForSlot(AstNode* node, Operand* op, Register base) {
  Operand* stored = slot_;
  Register stored_base = result_;
  VisitorType stored_type = visitor_type_;

  slot_ = op;
  result_ = base;
  visitor_type_ = kSlot;

  AstNode* result = Visit(node);

  slot_ = stored;
  result_ = stored_base;
  visitor_type_ = stored_type;

  return result;
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
  push(rbx);

  Operand lhs(rax, 0);
  VisitForSlot(stmt->lhs(), &lhs, rax);
  pop(rbx);

  // Put value into slot
  movq(lhs, rbx);

  movq(result(), rbx);

  return stmt;
}


AstNode* Fullgen::VisitValue(AstNode* node) {
  AstValue* value = AstValue::Cast(node);

  // Get pointer to slot first
  if (value->slot()->is_stack()) {
    // On stack variables
    slot()->base(rbp);
    slot()->scale(Operand::one);
    slot()->disp(-sizeof(void*) * (value->slot()->index() + 1));
  } else {
    // Context variables
    movq(result(), rsi);

    // Lookup context
    int32_t depth = value->slot()->depth();
    while (--depth >= 0) {
      Operand parent(result(), 8);
      movq(result(), parent);
    }

    slot()->base(result());
    slot()->scale(Operand::one);
    // Skip tag and reference to parent scope
    slot()->disp(sizeof(void*) * (value->slot()->index() + 2));
  }

  if (visiting_for_value()) {
    movq(result(), *slot());
  }

  return node;
}


AstNode* Fullgen::VisitNumber(AstNode* node) {
  assert(visiting_for_value());

  Label runtime_alloc, finish;

  Register result_end = result().is(rbx) ? rax : rbx;
  Allocate(result(), result_end, 16, scratch, &runtime_alloc);

  Operand qtag(result(), 0);
  Operand qvalue(result(), 8);

  movq(qtag, Immediate(Heap::kNumber));
  movq(qvalue, Immediate(StringToInt(node->value_, node->length_)));

  jmp(&finish);
  bind(&runtime_alloc);

  // XXX: Allocate number or run GC here
  // actually number should be unboxed, but that'll be done later :)
  emitb(0xcc);

  bind(&finish);

  return node;
}


} // namespace dotlang
