#include "macroassembler-x64.h"
#include "fullgen.h"
#include "heap.h" // Heap
#include "ast.h" // AstNode
#include "zone.h" // ZoneObject
#include "utils.h" // List

#include <assert.h>
#include <stdint.h> // uint32_t
#include <stdlib.h> // NULL

namespace dotlang {


void Fullgen::FFunction::Use(uint32_t offset) {
  RelocationInfo* info = new RelocationInfo(
        RelocationInfo::kAbsolute,
        RelocationInfo::kQuad,
        offset - 8);
  uses_.Push(info);
  asm_->relocation_info_.Push(info);
}


void Fullgen::FFunction::Allocate(uint32_t addr) {
  List<RelocationInfo*, ZoneObject>::Item* item = uses_.head();
  while (item != NULL) {
    item->value()->target(addr);
    item = item->next();
  }
}


void Fullgen::Generate(AstNode* ast) {
  fns_.Push(new FFunction(this, FunctionLiteral::Cast(ast)));

  FFunction* fn;
  while ((fn = fns_.Shift()) != NULL) {
    // Align function if needed
    offset_ = RoundUp(offset_, 16);
    Grow();

    // Replace all function's uses by generated address
    fn->Allocate(offset());

    // Generate function's body
    GeneratePrologue(fn->fn());
    VisitChildren(fn->fn());

    // In case if function doesn't have `return` statements
    // we should still return `nil` value
    movq(rax, 0);
    GenerateEpilogue();
  }
}


void Fullgen::GeneratePrologue(AstNode* stmt) {
  // rdi <- context (if non-root)
  push(rbp);
  push(rbx); // callee-save
  movq(rbp, rsp);

  // Allocate space for on stack variables
  // and align stack
  subq(rsp,
       Immediate(8 + RoundUp((stmt->stack_slots() + 1) * sizeof(void*), 16)));

  // Main function should allocate context for itself
  // All other functions will receive context in rdi
  if (stmt->is_root()) {
    AllocateContext(rax, rbx, scratch, stmt->context_slots());
    // Set root context
    movq(rdi, rax);
  }
}


void Fullgen::GenerateEpilogue() {
  // rax will hold result of function
  movq(rsp, rbp);
  pop(rbx);
  pop(rbp);

  ret(0);
}


AstNode* Fullgen::VisitForValue(AstNode* node, Register reg) {
  // Save previous data
  Register stored = result_;
  VisitorType stored_type = visitor_type_;

  // Set new
  result_ = reg;
  visitor_type_ = kValue;

  // Visit node
  AstNode* result = Visit(node);

  // Restore
  result_ = stored;
  visitor_type_ = stored_type;

  return result;
}


AstNode* Fullgen::VisitForSlot(AstNode* node, Operand* op, Register base) {
  // Save data
  Operand* stored = slot_;
  Register stored_base = result_;
  VisitorType stored_type = visitor_type_;

  // Set new
  slot_ = op;
  result_ = base;
  visitor_type_ = kSlot;

  // Visit node
  AstNode* result = Visit(node);

  // Restore
  slot_ = stored;
  result_ = stored_base;
  visitor_type_ = stored_type;

  return result;
}


AstNode* Fullgen::VisitFunction(AstNode* stmt) {
  FunctionLiteral* fn = FunctionLiteral::Cast(stmt);
  FFunction* ffn = new FFunction(this, fn);
  fns_.Push(ffn);

  push(rax);
  push(rbx);
  push(rcx);

  movq(rcx, Immediate(0));
  ffn->Use(offset());

  ChangeAlign(3);

  AllocateContext(rax, rbx, scratch, stmt->context_slots());

  // Put address of code chunk into second slot
  Operand code(rax, 16);
  movq(code, rcx);

  if (visiting_for_value()) {
    movq(result(), rax);
  } else {
    // TODO: There should be a runtime error, not assertion
    // Or a parse error
    assert(fn->variable() != NULL);

    // Get slot
    Operand name(rax, 0);
    VisitForSlot(fn->variable(), &name, scratch);

    // Put context into slot
    movq(name, rax);
  }

  ChangeAlign(-3);
  pop(rcx);
  pop(rbx);
  pop(rax);

  return stmt;
}


AstNode* Fullgen::VisitCall(AstNode* stmt) {
  FunctionLiteral* fn = FunctionLiteral::Cast(stmt);

  // Save rax if we're not going to overwrite it
  if (!visiting_for_value() || !result().is(rax)) {
    push(rax);
  }

  // Get pointer to function in a heap
  assert(fn->variable() != NULL);
  VisitForValue(fn->variable(), scratch);

  // Generate calling code
  Call(scratch);

  // Restore rax and set result if needed
  if (visiting_for_value()) {
    if (!result().is(rax)) {
      movq(result(), rax);
      pop(rax);
    }
  } else {
    pop(rax);
  }

  return stmt;
}


AstNode* Fullgen::VisitAssign(AstNode* stmt) {
  // Get value of right-hand side expression in rbx
  VisitForValue(stmt->rhs(), rbx);
  push(rbx);

  // Get target slot for left-hand side
  Operand lhs(rax, 0);
  ChangeAlign(1);
  VisitForSlot(stmt->lhs(), &lhs, scratch);
  ChangeAlign(-1);
  pop(rbx);

  // Put value into slot
  movq(lhs, rbx);

  // Propagate result of assign operation
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
    movq(result(), rdi);

    // Lookup context
    int32_t depth = value->slot()->depth();
    while (--depth >= 0) {
      Operand parent(result(), 8);
      movq(result(), parent);
    }

    slot()->base(result());
    slot()->scale(Operand::one);
    // Skip tag, code addr and reference to parent scope
    slot()->disp(sizeof(void*) * (value->slot()->index() + 3));
  }

  // If we was asked to return value - dereference slot
  if (visiting_for_value()) {
    movq(result(), *slot());
  }

  return node;
}


AstNode* Fullgen::VisitNumber(AstNode* node) {
  assert(visiting_for_value());

  Register result_end = result().is(rbx) ? rax : rbx;
  AllocateNumber(result(),
                 result_end,
                 scratch,
                 StringToInt(node->value(), node->length()));
  return node;
}


AstNode* Fullgen::VisitReturn(AstNode* node) {
  if (node->lhs() != NULL) {
    // Get value of expression
    VisitForValue(node->lhs(), rax);
  } else {
    // Or just nullify output
    movq(rax, Immediate(0));
  }

  GenerateEpilogue();

  return node;
}


} // namespace dotlang
