#include "macroassembler-x64.h"
#include "fullgen.h"
#include "heap.h" // Heap
#include "ast.h" // AstNode
#include "zone.h" // ZoneObject
#include "stubs.h" // Stubs
#include "runtime.h" // Runtime functions
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
                               visitor_type_(kSlot),
                               current_function_(NULL) {
  stubs()->fullgen(this);
}


void Fullgen::DotFunction::Generate() {
  // Generate function's body
  fullgen()->GeneratePrologue(fn());
  fullgen()->VisitChildren(fn());

  // In case if function doesn't have `return` statements
  // we should still return `nil` value
  masm()->movq(rax, 0);

  fullgen()->GenerateEpilogue(fn());
}


void Fullgen::Generate(AstNode* ast) {
  fns_.Push(new DotFunction(this, FunctionLiteral::Cast(ast)));

  FFunction* fn;
  while ((fn = fns_.Shift()) != NULL) {
    current_function(DotFunction::Cast(fn));

    // Align function if needed
    AlignCode();

    // Replace all function's uses by generated address
    fn->Allocate(offset());

    // Generate functions' body
    fn->Generate();
  }
}


void Fullgen::GeneratePrologue(AstNode* stmt) {
  // rdi <- reference to parent context (zero for root)
  // rsi <- arguments count
  // rdx <- (root only) address of root context
  push(rbp);
  push(rbx);

  // Store callee-save registers only on C++ - Dotlang boundary
  if (stmt->is_root()) {
    push(r12);
    push(r13);
    push(r14);
    push(r15);

    // Store address of root context
    movq(root_reg, rdx);

    // Nullify all registers to help GC distinguish on-stack values
    xorq(rax, rax);
    xorq(rbx, rbx);
    xorq(rcx, rcx);
    xorq(rdx, rdx);
    xorq(r8, r8);
    xorq(r9, r9);
    // r10 is a root register
    xorq(r11, r11);
    xorq(r12, r12);
    xorq(r13, r13);
    xorq(r14, r14);
    xorq(r15, r15);
  }
  movq(rbp, rsp);

  // Allocate space for on stack variables
  // and align stack
  uint32_t on_stack_size = 8 + RoundUp((stmt->stack_slots() + 1) * 8, 16);
  subq(rsp, Immediate(on_stack_size));

  // Allocate context and clear stack slots
  AllocateContext(stmt->context_slots());
  FillStackSlots(on_stack_size >> 3);

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
    Operand rhs(rbp, 24 + 8 * i++);

    cmpq(rsi, Immediate(i));
    jmp(kLt, &body);

    VisitForSlot(item->value(), &lhs, scratch);
    movq(scratch, rhs);
    movq(lhs, scratch);

    item = item->next();
  }

  bind(&body);

  // Cleanup junk
  xorq(rsi, rsi);
  xorq(scratch, scratch);
}


void Fullgen::GenerateEpilogue(AstNode* stmt) {
  // rax will hold result of function
  movq(rsp, rbp);

  // Restore callee save registers
  if (stmt->is_root()) {
    pop(r15);
    pop(r14);
    pop(r13);
    pop(r12);
  }
  pop(rbx);
  pop(rbp);

  ret(0);
}


void Fullgen::PlaceInRoot(char* addr) {
  Operand root_op(root_reg, 8 * (3 + root_context()->length()));
  movq(result(), root_op);

  root_context()->Push(addr);
}


char* Fullgen::AllocateRoot() {
  return HContext::New(heap(), NULL, root_context());
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
      Throw(Heap::kErrorIncorrectLhs);
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

  Label not_function(this), done(this);

  // Get pointer to function in a heap
  if (fn->variable() == NULL) {
    Throw(Heap::kErrorCallWithoutVariable);
  } else {
    AstNode* name = AstValue::Cast(fn->variable())->name();

    // handle __$gc() call
    if (fn->variable()->is(AstNode::kValue) &&
        name->length() == 5 && strncmp(name->value(), "__$gc", 5) == 0) {
      RuntimeCollectGarbageCallback gc = &RuntimeCollectGarbage;
      Pushad();

      Align a(this);

      // RuntimeCollectGarbage(heap, stack_top)
      movq(rsi, rsp);
      movq(rdi, Immediate(reinterpret_cast<uint64_t>(heap())));

      movq(rax, Immediate(*reinterpret_cast<uint64_t*>(&gc)));
      Call(rax);

      Popad(reg_nil);
      movq(result(), Immediate(0));

      return stmt;
    }

    // Save rax if we're not going to overwrite it
    Save(rax);

    // Save old context
    Save(rdi);

    VisitForValue(fn->variable(), rax);
    IsNil(rax, NULL, &not_function);
    IsHeapObject(Heap::kTagFunction, rax, &not_function, NULL);

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

      if (fn->args()->length() != 0) {
        // Unwind stack
        addq(rsp, Immediate(fn->args()->length() * 8));
      }
    }

    // Finally restore everything
    ChangeAlign(-fn->args()->length());
    Restore(rdi);

    // Restore rax and set result if needed
    Result(rax);
    Restore(rax);
  }

  jmp(&done);
  bind(&not_function);

  movq(result(), Immediate(Heap::kTagNil));

  bind(&done);

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

  // If it's Fullgen generated AST Value
  if (value->is_register()) {
    assert(visiting_for_value());
    movq(result(), FAstRegister::Cast(value)->reg());
    return node;
  }

  if (value->is_operand()) {
    assert(visiting_for_slot());
    slot()->base(FAstOperand::Cast(value)->op()->base());
    slot()->disp(FAstOperand::Cast(value)->op()->disp());
    return node;
  }

  slot()->scale(Operand::one);

  // Get pointer to slot first
  if (value->slot()->is_stack()) {
    // On stack variables
    slot()->base(rbp);
    slot()->disp(-8 * (value->slot()->index() + 1));
  } else {
    int32_t depth = value->slot()->depth();

    if (depth == -2) {
      // Root register lookup
      slot()->base(root_reg);
      slot()->disp(8 * (value->slot()->index() + 3));
    } else if (depth == -1) {
      // Global lookup
      // TODO: Implement me
      emitb(0xcc);
    } else {
      // Context variables
      movq(result(), rdi);

      // Lookup context
      while (--depth >= 0) {
        Operand parent(result(), 8);
        movq(result(), parent);
      }

      slot()->base(result());
      slot()->scale(Operand::one);
      // Skip tag, code addr and reference to parent scope
      slot()->disp(8 * (value->slot()->index() + 3));
    }
  }

  // If we was asked to return value - dereference slot
  if (visiting_for_value()) {
    movq(result(), *slot());
  }

  return node;
}


AstNode* Fullgen::VisitMember(AstNode* node) {
  Label nil_error(this), non_object_error(this), done(this);

  VisitForValue(node->lhs(), result());

  // Throw error if we're trying to lookup into nil object
  IsNil(result(), NULL, &non_object_error);
  IsUnboxed(result(), NULL, &non_object_error);

  // Or into non-object
  IsHeapObject(Heap::kTagObject, result(), &non_object_error, NULL);

  // Calculate hash of property

  Save(rax);
  {
    // Stub(change, property, object)
    ChangeAlign(3);
    Align a(this);

    push(result());

    VisitForValue(node->rhs(), result());
    push(result());

    movq(result(), Immediate(visiting_for_slot()));
    push(result());

    Call(stubs()->GetLookupPropertyStub());
    // Stub will unwind stack automatically
    ChangeAlign(-3);
  }
  Result(rax);
  Restore(rax);

  slot()->base(result());
  slot()->disp(0);

  // Unbox value if asked
  if (visiting_for_value()) {
    movq(result(), *slot());
  }

  jmp(&done);

  bind(&non_object_error);

  // Non object lookups will return nil
  movq(result(), Immediate(Heap::kTagNil));

  bind(&done);

  return node;
}


AstNode* Fullgen::VisitNumber(AstNode* node) {
  if (!visiting_for_value()) {
    Throw(Heap::kErrorIncorrectLhs);
    return node;
  }

  // TODO: Support heap numbers too
  int64_t value = StringToInt(node->value(), node->length());
  movq(result(), Immediate(TagNumber(value)));

  return node;
}


AstNode* Fullgen::VisitString(AstNode* node) {
  if (visiting_for_slot()) {
    Throw(Heap::kErrorIncorrectLhs);
    return node;
  }

  PlaceInRoot(HString::New(heap(), NULL, node->value(), node->length()));

  return node;
}


AstNode* Fullgen::VisitProperty(AstNode* node) {
  // kProperty is essentially the same as string
  return VisitString(node);
}


void Fullgen::ConvertToBoolean() {
  Label not_bool(this), coerced_type(this);

  // Check type and coerce if not boolean
  IsNil(result(), NULL, &not_bool);
  IsUnboxed(result(), NULL, &not_bool);
  IsHeapObject(Heap::kTagBoolean, result(), &not_bool, NULL);

  jmp(&coerced_type);
  bind(&not_bool);

  // Coerce type to boolean
  Save(rax);
  {
    // Stub(value)
    ChangeAlign(1);
    Align a(this);

    push(result());
    Call(stubs()->GetCoerceToBooleanStub());
    // Stub will unwind stack automatically
    ChangeAlign(-1);
  }
  Result(rax);
  Restore(rax);

  bind(&coerced_type);
}


AstNode* Fullgen::VisitIf(AstNode* node) {
  Label fail_body(this), done(this);

  AstNode* expr = node->lhs();
  AstNode* success = node->rhs();
  AstList::Item* fail_item = node->children()->head()->next()->next();
  AstNode* fail = NULL;
  if (fail_item != NULL) fail = fail_item->value();

  VisitForValue(expr, result());

  ConvertToBoolean();

  IsTrue(result(), &fail_body, NULL);

  VisitForValue(success, result());

  jmp(&done);
  bind(&fail_body);

  if (fail != NULL) VisitForValue(fail, result());

  bind(&done);

  return node;
}


AstNode* Fullgen::VisitWhile(AstNode* node) {
  Label loop_start(this), loop_end(this);

  AstNode* expr = node->lhs();
  AstNode* body = node->rhs();

  bind(&loop_start);

  VisitForValue(expr, result());

  ConvertToBoolean();

  IsTrue(result(), &loop_end, NULL);

  VisitForValue(body, result());

  jmp(&loop_start);

  bind(&loop_end);

  return node;
}


AstNode* Fullgen::VisitNil(AstNode* node) {
  if (visiting_for_slot()) {
    Throw(Heap::kErrorIncorrectLhs);
    return node;
  }

  movq(result(), Immediate(0));

  return node;
}


AstNode* Fullgen::VisitTrue(AstNode* node) {
  if (visiting_for_slot()) {
    Throw(Heap::kErrorIncorrectLhs);
    return node;
  }

  PlaceInRoot(HBoolean::New(heap(), NULL, true));

  return node;
}


AstNode* Fullgen::VisitFalse(AstNode* node) {
  if (visiting_for_slot()) {
    Throw(Heap::kErrorIncorrectLhs);
    return node;
  }

  PlaceInRoot(HBoolean::New(heap(), NULL, false));

  return node;
}


AstNode* Fullgen::VisitObjectLiteral(AstNode* node) {
  if (visiting_for_slot()) {
    Throw(Heap::kErrorIncorrectLhs);
    return node;
  }

  ObjectLiteral* obj = ObjectLiteral::Cast(node);

  Save(rax);
  Save(rbx);

  // Ensure that map will be filled only by half at maximum
  movq(rbx, Immediate(TagNumber(PowerOfTwo(node->children()->length() << 1))));
  AllocateObjectLiteral(rbx, rax);

  // Set every key/value pair
  assert(obj->keys()->length() == obj->values()->length());
  AstList::Item* key = obj->keys()->head();
  AstList::Item* value = obj->values()->head();
  while (key != NULL) {
    AstNode* member = new AstNode(AstNode::kMember);
    member->children()->Push(new FAstRegister(rax));
    member->children()->Push(key->value());

    AstNode* assign = new AstNode(AstNode::kAssign);
    assign->children()->Push(member);
    assign->children()->Push(value->value());

    VisitForValue(assign, rbx);

    key = key->next();
    value = value->next();
  }

  Result(rax);
  Restore(rax);
  Restore(rbx);

  return node;
}


AstNode* Fullgen::VisitArrayLiteral(AstNode* node) {
  if (visiting_for_slot()) {
    Throw(Heap::kErrorIncorrectLhs);
    return node;
  }

  Save(rax);
  Save(rbx);

  // Ensure that map will be filled only by half at maximum
  // (items + `length` property)
  movq(rbx,
       Immediate(TagNumber(PowerOfTwo((node->children()->length() + 1) << 1))));
  AllocateObjectLiteral(rbx, rax);

  AstList::Item* item = node->children()->head();
  uint64_t index = 0;
  while (item != NULL) {
    char keystr[32];
    AstNode* key = new AstNode(AstNode::kProperty);
    key->value(keystr);
    key->length(IntToString(index, keystr));

    AstNode* member = new AstNode(AstNode::kMember);
    member->children()->Push(new FAstRegister(rax));
    member->children()->Push(key);

    AstNode* assign = new AstNode(AstNode::kAssign);
    assign->children()->Push(member);
    assign->children()->Push(item->value());

    VisitForValue(assign, rbx);

    item = item->next();
    index++;
  }

  {
    // Set `length`
    AstNode* key = new AstNode(AstNode::kProperty);
    key->value("length");
    key->length(6);

    char lenstr[32];
    AstNode* value = new AstNode(AstNode::kNumber);
    value->value(lenstr);
    value->length(IntToString(index, lenstr));

    // arr.length = num
    AstNode* member = new AstNode(AstNode::kMember);
    member->children()->Push(new FAstRegister(rax));
    member->children()->Push(key);

    AstNode* assign = new AstNode(AstNode::kAssign);
    assign->children()->Push(member);
    assign->children()->Push(value);

    VisitForValue(assign, rbx);
  }

  Result(rax);
  Restore(rax);
  Restore(rbx);

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

  GenerateEpilogue(current_function()->fn());

  return node;
}


AstNode* Fullgen::VisitUnOp(AstNode* node) {
  UnOp* op = UnOp::Cast(node);
  Label done(this);

  if (visiting_for_slot()) {
    Throw(Heap::kErrorIncorrectLhs);
    return node;
  }

  // Changing ops should be translated into another form
  if (op->is_changing()) {
    AstNode* rhs = NULL;
    AstNode* one = new AstNode(AstNode::kNumber);
    one->value("1");
    one->length(1);

    switch (op->subtype()) {
     case UnOp::kPreInc:
     case UnOp::kPostInc:
      rhs = new BinOp(BinOp::kAdd, op->lhs(), one);
      break;
     case UnOp::kPreDec:
     case UnOp::kPostDec:
      rhs = new BinOp(BinOp::kSub, op->lhs(), one);
      break;
     default:
      break;
    }

    AstNode* assign = new AstNode(AstNode::kAssign);
    assign->children()->Push(op->lhs());
    assign->children()->Push(rhs);

    // ++a => a = a + 1
    if (op->subtype() == UnOp::kPreInc || op->subtype() == UnOp::kPreDec) {
      Visit(assign);
      return node;
    }

    // a++ => $scratch = a; a = $scratch + 1; $scratch
    Operand result_slot(result(), 0);
    VisitForSlot(op->lhs(), &result_slot, result());

    movq(scratch, result_slot);

    Push(scratch);
    Save(rax);
    Save(rbx);

    // Put slot into rax
    movq(rax, result_slot.base());
    result_slot.base(rax);

    // Put slot's value into rbx
    movq(rbx, result_slot);

    assign->children()->head()->value(new FAstOperand(&result_slot));
    rhs->children()->head()->value(new FAstRegister(rbx));
    VisitForValue(assign, result());

    Restore(rbx);
    Restore(rax);
    Pop(result());

    return node;
  }

  // For `nil` any unop will return `nil`
  VisitForValue(op->lhs(), result());
  IsNil(result(), NULL, &done);

  // TODO: Coerce to expected type and perform operation
  emitb(0xcc);

  bind(&done);
  return node;
}


AstNode* Fullgen::VisitBinOp(AstNode* node) {
  BinOp* op = BinOp::Cast(node);

  if (visiting_for_slot()) {
    Throw(Heap::kErrorIncorrectLhs);
    return node;
  }

  Save(rax);
  Save(rbx);

  Label nil_result(this), not_unboxed(this), done(this);

  VisitForValue(op->lhs(), rbx);

  IsNil(rbx, NULL, &nil_result);

  VisitForValue(op->rhs(), rax);

  IsUnboxed(rbx, &not_unboxed, NULL);

  {
    Untag(rax);
    Untag(rbx);

    // Number (+) Number
    movq(scratch, rbx);

    switch (op->subtype()) {
     case BinOp::kAdd:
      addq(scratch, rax);
      break;
     case BinOp::kSub:
      subq(scratch, rax);
      break;
     default:
      emitb(0xcc);
      break;
    }
    TagNumber(scratch);
    movq(result(), scratch);

    jmp(&done);
  }

  bind(&not_unboxed);

  {
    // TODO: Coerce here if needed and implement other binary ops
    emitb(0xcc);
  }

  jmp(&done);
  bind(&nil_result);

  {
    // nil (+) nil = nil
    movq(result(), Immediate(Heap::kTagNil));
  }

  bind(&done);

  Restore(rbx);
  Restore(rax);

  return node;
}


} // namespace dotlang
