#include "macroassembler-x64.h"
#include "fullgen.h"
#include "heap.h" // Heap
#include "ast.h" // AstNode
#include "zone.h" // ZoneObject
#include "stubs.h" // Stubs
#include "utils.h" // List

#include <assert.h>
#include <stdint.h> // uint32_t
#include <stdlib.h> // NULL

namespace dotlang {


void FFunction::Use(uint32_t offset) {
  RelocationInfo* info = new RelocationInfo(
        RelocationInfo::kAbsolute,
        RelocationInfo::kQuad,
        offset - 8);
  if (addr_ != 0) info->target(addr_);
  uses_.Push(info);
  masm()->relocation_info_.Push(info);
}


void FFunction::Allocate(uint32_t addr) {
  assert(addr_ == 0);
  addr_ = addr;
  List<RelocationInfo*, ZoneObject>::Item* item = uses_.head();
  while (item != NULL) {
    item->value()->target(addr);
    item = item->next();
  }
}


Fullgen::Fullgen(Heap* heap) : Masm(heap),
                               Visitor(kPreorder),
                               heap_(heap),
                               visitor_type_(kSlot) {
  stubs()->fullgen(this);
}


void Fullgen::DotFunction::Generate() {
  // Generate function's body
  fullgen()->GeneratePrologue(fn());
  fullgen()->VisitChildren(fn());

  // In case if function doesn't have `return` statements
  // we should still return `nil` value
  masm()->movq(rax, 0);

  fullgen()->GenerateEpilogue();
}


void Fullgen::Generate(AstNode* ast) {
  fns_.Push(new DotFunction(this, FunctionLiteral::Cast(ast)));

  FFunction* fn;
  while ((fn = fns_.Shift()) != NULL) {
    // Align function if needed
    AlignCode();

    // Replace all function's uses by generated address
    fn->Allocate(offset());

    // Generate functions' body
    fn->Generate();
  }
}


void Fullgen::GeneratePrologue(AstNode* stmt) {
  // rdi <- reference to parent context (if non-root)
  // rsi <- arguments count
  push(rbp);
  push(rbx); // callee-save
  movq(rbp, rsp);

  // Allocate space for on stack variables
  // and align stack
  subq(rsp,
       Immediate(8 + RoundUp((stmt->stack_slots() + 1) * sizeof(void*), 16)));

  // Allocate context and clear stack slots
  AllocateContext(stmt->context_slots());
  FillStackSlots(stmt->stack_slots());

  // Store root stack address(rbp) to heap
  // It's needed to unwind stack on exceptions
  if (stmt->is_root()) StoreRootStack();

  // Place all arguments into their slots
  Label body(this);

  FunctionLiteral* fn = FunctionLiteral::Cast(stmt);
  AstList::Item* item = fn->args()->head();
  uint32_t i = 0;
  while (item != NULL) {
    Operand lhs(rax, 0);
    Operand rhs(rbp, 24 + sizeof(void*) * i++);

    cmp(rsi, Immediate(i));
    jmp(kLt, &body);

    VisitForSlot(item->value(), &lhs, scratch);
    movq(scratch, rhs);
    movq(lhs, scratch);

    item = item->next();
  }

  bind(&body);
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
  FFunction* ffn = new DotFunction(this, fn);
  fns_.Push(ffn);

  Save(rax);
  Save(rcx);

  movq(rcx, Immediate(0));
  ffn->Use(offset());

  // Allocate function object that'll reference to current scope
  // and have address of actual code
  AllocateFunction(rcx, rax);

  if (visiting_for_value()) {
    Result(rax);
  } else {
    // Declaration of anonymous function is impossible
    if (fn->variable() == NULL) {
      Throw(Heap::kErrorUnexpectedFunction);
    } else {
      // Get slot
      Operand name(rax, 0);
      VisitForSlot(fn->variable(), &name, rcx);

      // Put context into slot
      movq(name, rax);
    }
  }

  Restore(rcx);
  Restore(rax);

  return stmt;
}


AstNode* Fullgen::VisitCall(AstNode* stmt) {
  FunctionLiteral* fn = FunctionLiteral::Cast(stmt);

  // Get pointer to function in a heap
  if (fn->variable() == NULL) {
    Throw(Heap::kErrorCallWithoutVariable);
  } else {
    // Save rax if we're not going to overwrite it
    Save(rax);

    // Save old context
    Save(rdi);

    VisitForValue(fn->variable(), rax);

    ChangeAlign(fn->args()->length());

    {
      Align a(this);

      AstList::Item* item = fn->args()->head();
      while (item != NULL) {
        VisitForValue(item->value(), rsi);

        // Push argument and change alignment
        push(rsi);
        ChangeAlign(1);
        item = item->next();
      }
      // Restore alignment
      ChangeAlign(-fn->args()->length());

      // Generate calling code
      Call(rax, fn->args()->length());
    }

    // Finally restore everything
    ChangeAlign(-fn->args()->length());
    Restore(rdi);

    // Restore rax and set result if needed
    Result(rax);
    Restore(rax);
  }

  return stmt;
}


AstNode* Fullgen::VisitAssign(AstNode* stmt) {
  Save(rax);
  Save(rbx);

  // Get value of right-hand side expression in rbx
  VisitForValue(stmt->rhs(), rbx);

  // Get target slot for left-hand side
  Operand lhs(rax, 0);
  VisitForSlot(stmt->lhs(), &lhs, rax);

  // Put value into slot
  movq(lhs, rbx);

  // Propagate result of assign operation
  Result(rbx);
  Restore(rbx);
  Restore(rax);

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
    slot()->disp(sizeof(void*) * (value->slot()->index() + 2));
  }

  // If we was asked to return value - dereference slot
  if (visiting_for_value()) {
    movq(result(), *slot());
  }

  return node;
}


AstNode* Fullgen::VisitNumber(AstNode* node) {
  assert(visiting_for_value());

  movq(scratch, Immediate(StringToInt(node->value(), node->length())));
  AllocateNumber(scratch, result());

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


AstNode* Fullgen::VisitAdd(AstNode* node) {
  Save(rax);
  Save(rbx);

  Label not_number(this), done(this);

  VisitForValue(node->lhs(), rax);
  VisitForValue(node->rhs(), rbx);

  IsHeapObject(Heap::kTagNumber, rax, &not_number);
  IsHeapObject(Heap::kTagNumber, rbx, &not_number);

  UnboxNumber(rax);
  UnboxNumber(rbx);

  movq(scratch, rax);
  addq(scratch, rbx);
  AllocateNumber(scratch, result());

  jmp(&done);
  bind(&not_number);
  emitb(0xcc);

  bind(&done);

  Restore(rbx);
  Restore(rax);

  return node;
}


} // namespace dotlang
