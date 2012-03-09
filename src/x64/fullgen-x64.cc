#include "fullgen.h"
#include "macroassembler.h"
#include "code-space.h" // CodeSpace
#include "heap.h" // Heap
#include "heap-inl.h"
#include "ast.h" // AstNode
#include "zone.h" // ZoneObject
#include "stubs.h" // Stubs
#include "utils.h" // List

#include <assert.h>
#include <stdint.h> // uint32_t
#include <stdlib.h> // NULL
#include <stdio.h> // snprintf

namespace candor {
namespace internal {

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


Fullgen::Fullgen(CodeSpace* space) : Masm(space),
                                     Visitor(kPreorder),
                                     space_(space),
                                     visitor_type_(kValue),
                                     current_function_(NULL) {
  // Create a `global` object
  root_context()->Push(HObject::NewEmpty(heap()));

  // Place some root values
  root_context()->Push(HNil::New(heap()));
  root_context()->Push(HBoolean::New(heap(), Heap::kTenureOld, true));
  root_context()->Push(HBoolean::New(heap(), Heap::kTenureOld, false));

  // Place types
  root_context()->Push(HString::New(heap(), Heap::kTenureOld, "nil", 3));
  root_context()->Push(HString::New(heap(), Heap::kTenureOld, "boolean", 7));
  root_context()->Push(HString::New(heap(), Heap::kTenureOld, "number", 6));
  root_context()->Push(HString::New(heap(), Heap::kTenureOld, "string", 6));
  root_context()->Push(HString::New(heap(), Heap::kTenureOld, "object", 6));
  root_context()->Push(HString::New(heap(), Heap::kTenureOld, "array", 5));
  root_context()->Push(HString::New(heap(), Heap::kTenureOld, "function", 8));
  root_context()->Push(HString::New(heap(), Heap::kTenureOld, "cdata", 5));
}


void Fullgen::CandorFunction::Generate() {
  // Generate function's body
  fullgen()->GeneratePrologue(fn());
  fullgen()->VisitChildren(fn());

  // In case if function doesn't have `return` statements
  // we should still return `nil` value
  masm()->movq(rax, 0);

  fullgen()->GenerateEpilogue(fn());
}


void Fullgen::Throw(Heap::Error err) {
  // TODO: set error flag
  emitb(0xcc);
}


void Fullgen::Generate(AstNode* ast) {
  fns_.Push(new CandorFunction(this, FunctionLiteral::Cast(ast)));

  FFunction* fn;
  while ((fn = fns_.Shift()) != NULL) {
    current_function(CandorFunction::Cast(fn));

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
  // rsi <- unboxed arguments count (tagged)
  push(rbp);
  movq(rbp, rsp);

  // Allocate space for on stack variables
  // and align stack
  uint32_t on_stack_size = RoundUp(8 + (stmt->stack_slots() + 1) * 8, 16);
  subq(rsp, Immediate(on_stack_size));

  // Allocate context and clear stack slots
  AllocateContext(stmt->context_slots());
  FillStackSlots(on_stack_size >> 3);

  // Place all arguments into their slots
  Label body(this);

  FunctionLiteral* fn = FunctionLiteral::Cast(stmt);
  AstList::Item* item = fn->args()->head();
  uint32_t i = 0;
  while (item != NULL) {
    Operand lhs(rax, 0);
    Operand rhs(rbp, 8 * (2 + fn->args()->length() - ++i));

    cmpq(rsi, Immediate(TagNumber(i)));
    jmp(kLt, &body);

    VisitForSlot(item->value(), &lhs, scratch);
    movq(rdx, rhs);
    movq(lhs, rdx);

    item = item->next();
  }

  bind(&body);

  // Cleanup junk
  xorq(rdx, rdx);
  xorq(scratch, scratch);
}


void Fullgen::GenerateEpilogue(AstNode* stmt) {
  // rax will hold result of function
  movq(rsp, rbp);
  pop(rbp);

  ret(0);
}


void Fullgen::PlaceInRoot(char* addr) {
  Operand root_op(root_reg, 8 * (3 + root_context()->length()));
  movq(result(), root_op);

  root_context()->Push(addr);
}


char* Fullgen::AllocateRoot() {
  return HContext::New(heap(), root_context());
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
  FFunction* ffn = new CandorFunction(this, fn);
  fns_.Push(ffn);

  Save(rax);
  Save(rcx);

  movq(rcx, Immediate(0));
  ffn->Use(offset());

  // Allocate function object that'll reference to current scope
  // and have address of actual code
  AllocateFunction(rcx, rax);

  if (visiting_for_value() && fn->variable() == NULL) {
    Result(rax);
  } else {
    AstNode* assign = new AstNode(AstNode::kAssign);
    assign->children()->Push(fn->variable());
    assign->children()->Push(new FAstRegister(rax));

    Visit(assign);
  }

  Restore(rcx);
  Restore(rax);

  return stmt;
}


AstNode* Fullgen::VisitCall(AstNode* stmt) {
  FunctionLiteral* fn = FunctionLiteral::Cast(stmt);

  if (fn->variable() == NULL) {
    Throw(Heap::kErrorCallWithoutVariable);
    return stmt;
  }

  if (!visiting_for_value()) {
    Throw(Heap::kErrorIncorrectLhs);
    return stmt;
  }

  Label not_function(this), done(this);

  AstNode* name = AstValue::Cast(fn->variable())->name();

  // handle __$gc() call
  if (fn->variable()->is(AstNode::kValue) &&
      name->length() == 5 && strncmp(name->value(), "__$gc", 5) == 0) {
    Call(stubs()->GetCollectGarbageStub());
    movq(result(), Immediate(0));

    return stmt;
  }

  // Save rax if we're not going to overwrite it
  Save(rax);

  VisitForValue(fn->variable(), rax);
  IsNil(rax, NULL, &not_function);
  IsUnboxed(rax, NULL, &not_function);
  IsHeapObject(Heap::kTagFunction, rax, &not_function, NULL);

  Push(rsi);
  Push(rdi);
  Push(root_reg);
  {
    ChangeAlign(fn->args()->length());
    Align a(this);
    ChangeAlign(-fn->args()->length());

    AstList::Item* item = fn->args()->head();
    while (item != NULL) {
      {
        Align a(this);
        VisitForValue(item->value(), rsi);
      }

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
  Pop(root_reg);
  Pop(rdi);
  Pop(rsi);

  Result(rax);

  jmp(&done);
  bind(&not_function);

  movq(result(), Immediate(Heap::kTagNil));

  bind(&done);

  Restore(rax);

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
    slot()->base(FAstOperand::Cast(value)->op()->base());
    slot()->disp(FAstOperand::Cast(value)->op()->disp());

    if (visiting_for_value()) {
      movq(result(), *slot());
    }

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
      slot()->base(root_reg);
      slot()->disp(HContext::GetIndexDisp(Heap::kRootGlobalIndex));

      AstNode* member = new AstNode(AstNode::kMember);
      member->children()->Push(new FAstOperand(slot()));
      AstNode* property = new AstNode(AstNode::kString);
      property->value(value->name()->value());
      property->length(value->name()->length());

      member->children()->Push(property);
      if (visiting_for_slot()) {
        VisitForSlot(member, slot(), result());
      } else {
        VisitForValue(member, result());
      }
      return node;
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
  Label is_object(this), non_object_error(this), done(this);

  VisitForValue(node->lhs(), result());

  // Throw error if we're trying to lookup into nil object
  IsNil(result(), NULL, &non_object_error);
  IsUnboxed(result(), NULL, &non_object_error);

  // Or into non-object
  IsHeapObject(Heap::kTagObject, result(), NULL, &is_object);
  IsHeapObject(Heap::kTagArray, result(), &non_object_error, NULL);

  bind(&is_object);

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
  if (visiting_for_value()) {
    movq(result(), Immediate(Heap::kTagNil));
  } else {
    movq(result(), root_reg);
    addq(result(), HContext::GetIndexDisp(Heap::kRootNilIndex));
  }

  bind(&done);

  return node;
}


AstNode* Fullgen::VisitNumber(AstNode* node) {
  if (!visiting_for_value()) {
    Throw(Heap::kErrorIncorrectLhs);
    return node;
  }

  if (StringIsDouble(node->value(), node->length())) {
    // Allocate boxed heap number
    double value = StringToDouble(node->value(), node->length());

    PlaceInRoot(HNumber::New(heap(), Heap::kTenureOld, value));
  } else {
    // Allocate unboxed number
    int64_t value = StringToInt(node->value(), node->length());
    movq(result(), Immediate(TagNumber(value)));
  }

  return node;
}


AstNode* Fullgen::VisitString(AstNode* node) {
  if (visiting_for_slot()) {
    Throw(Heap::kErrorIncorrectLhs);
    return node;
  }

  PlaceInRoot(HString::New(heap(),
                           Heap::kTenureOld,
                           node->value(),
                           node->length()));

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

  movq(result(), Immediate(Heap::kTagNil));

  return node;
}


AstNode* Fullgen::VisitTrue(AstNode* node) {
  if (visiting_for_slot()) {
    Throw(Heap::kErrorIncorrectLhs);
    return node;
  }

  Operand true_slot(root_reg, HContext::GetIndexDisp(Heap::kRootTrueIndex));
  movq(result(), true_slot);

  return node;
}


AstNode* Fullgen::VisitFalse(AstNode* node) {
  if (visiting_for_slot()) {
    Throw(Heap::kErrorIncorrectLhs);
    return node;
  }

  Operand false_slot(root_reg, HContext::GetIndexDisp(Heap::kRootFalseIndex));
  movq(result(), false_slot);

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
  AllocateObjectLiteral(Heap::kTagObject, rbx, rax);

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
  Restore(rbx);
  Restore(rax);

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
  movq(rbx,
       Immediate(TagNumber(PowerOfTwo(node->children()->length() << 1))));
  AllocateObjectLiteral(Heap::kTagArray, rbx, rax);

  AstList::Item* item = node->children()->head();
  uint64_t index = 0;
  while (item != NULL) {
    char keystr[32];
    AstNode* key = new AstNode(AstNode::kNumber);
    key->value(keystr);
    key->length(snprintf(keystr, sizeof(keystr), "%llu", index));

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
    movq(rax, Immediate(Heap::kTagNil));
  }

  GenerateEpilogue(current_function()->fn());

  return node;
}


AstNode* Fullgen::VisitTypeof(AstNode* node) {
  if (visiting_for_slot()) {
    Throw(Heap::kErrorIncorrectLhs);
    return node;
  }

  Save(rax);
  {
    Align a(this);

    VisitForValue(node->lhs(), rax);
    Call(stubs()->GetTypeofStub());
  }
  Result(rax);
  Restore(rax);

  return node;
}


AstNode* Fullgen::VisitSizeof(AstNode* node) {
  if (visiting_for_slot()) {
    Throw(Heap::kErrorIncorrectLhs);
    return node;
  }

  Save(rax);
  {
    Align a(this);

    VisitForValue(node->lhs(), rax);
    Call(stubs()->GetSizeofStub());
  }
  Result(rax);
  Restore(rax);

  return node;
}


AstNode* Fullgen::VisitKeysof(AstNode* node) {
  return node;
}


AstNode* Fullgen::VisitUnOp(AstNode* node) {
  UnOp* op = UnOp::Cast(node);

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

    // Get value
    movq(result(), result_slot);

    Push(result());
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

  } else if (op->subtype() == UnOp::kPlus || op->subtype() == UnOp::kMinus) {
    // +a = 0 + a
    // -a = 0 - a
    // TODO: Parser should genereate negative numbers where possible

    AstNode* zero = new AstNode(AstNode::kNumber);
    zero->value("0");
    zero->length(1);

    AstNode* wrap = new BinOp(
        op->subtype() == UnOp::kPlus ? BinOp::kAdd : BinOp::kSub,
        zero,
        op->lhs());

    VisitForValue(wrap, result());

  } else if (op->subtype() == UnOp::kNot) {
    // Get value and convert it to boolean
    VisitForValue(op->lhs(), result());
    ConvertToBoolean();

    Label done(this), ret_false(this);

    // Negate it
    IsTrue(result(), NULL, &ret_false);

    movq(scratch, Immediate(TagNumber(1)));

    jmp(&done);
    bind(&ret_false);
    movq(scratch, Immediate(TagNumber(0)));

    bind(&done);
    AllocateBoolean(scratch, result());
    xorq(scratch, scratch);

  } else {
    UNEXPECTED
  }

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

  {
    // lhs and rhs values will be pushed later
    ChangeAlign(2);
    Align a(this);

    Label not_unboxed(this), done(this);
    Label lhs_to_heap(this), rhs_to_heap(this);

    VisitForValue(op->lhs(), rax);
    VisitForValue(op->rhs(), rbx);

    // No need to change align here, because it was already changed above
    push(rax);
    push(rbx);

    IsNil(rax, NULL, &not_unboxed);
    IsNil(rbx, NULL, &not_unboxed);

    IsUnboxed(rax, &rhs_to_heap, NULL);
    IsUnboxed(rbx, &lhs_to_heap, NULL);

    // Number (+) Number
    if (BinOp::is_math(op->subtype())) {
      Untag(rax);
      Untag(rbx);

      switch (op->subtype()) {
       case BinOp::kAdd: addq(rax, rbx); break;
       case BinOp::kSub: subq(rax, rbx); break;
       case BinOp::kMul: push(rdx); imulq(rbx); pop(rdx); break;
       case BinOp::kDiv: push(rdx); idivq(rbx); pop(rdx); break;

       default: emitb(0xcc); break;
      }

      // Call stub on overflow
      jmp(kOverflow, &lhs_to_heap);

      TagNumber(rax);
      jmp(kCarry, &lhs_to_heap);

      movq(result(), rax);
    } else if (BinOp::is_binary(op->subtype())) {
      Untag(rax);
      Untag(rbx);

      switch (op->subtype()) {
       case BinOp::kBAnd: andq(rax, rbx); break;
       case BinOp::kBOr: orq(rax, rbx); break;
       case BinOp::kBXor: xorq(rax, rbx); break;

       default: emitb(0xcc); break;
      }

      TagNumber(rax);
      movq(result(), rax);
    } else if (BinOp::is_logic(op->subtype())) {
      Condition cond = BinOpToCondition(op->subtype(), kIntegral);
      // Note: rax and rbx are boxed here
      // Otherwise cmp won't work for negative numbers
      cmpq(rax, rbx);

      Label true_(this), cond_end(this);

      jmp(cond, &true_);

      movq(scratch, Immediate(TagNumber(0)));
      jmp(&cond_end);

      bind(&true_);
      movq(scratch, Immediate(TagNumber(1)));
      bind(&cond_end);

      AllocateBoolean(scratch, result());
    } else {
      // Call runtime for all other binary ops (boolean logic)
      jmp(&not_unboxed);
    }

    jmp(&done);

    bind(&lhs_to_heap);

    Pop(rbx);
    Pop(rax);

    Untag(rax);

    // Translate lhs to heap number
    xorqd(xmm1, xmm1);
    cvtsi2sd(xmm1, rax);
    xorq(rax, rax);
    AllocateNumber(xmm1, rax);

    // Replace on-stack value of rax
    Push(rax);
    Push(rbx);

    bind(&rhs_to_heap);

    // Check if rhs was unboxed after all
    // i.e. we may came from this case: 1 + 3.5
    IsUnboxed(rbx, &not_unboxed, NULL);

    Pop(rbx);

    Untag(rbx);

    // Translate rhs to heap number
    xorqd(xmm1, xmm1);
    cvtsi2sd(xmm1, rbx);
    xorq(rbx, rbx);

    AllocateNumber(xmm1, rbx);

    // Replace on-stack value of rbx
    Push(rbx);

    bind(&not_unboxed);

    char* stub = NULL;

#define BINARY_SUB_TYPES(V)\
    V(Add)\
    V(Sub)\
    V(Mul)\
    V(Div)\
    V(BAnd)\
    V(BOr)\
    V(BXor)\
    V(Eq)\
    V(StrictEq)\
    V(Ne)\
    V(StrictNe)\
    V(Lt)\
    V(Gt)\
    V(Le)\
    V(Ge)\
    V(LOr)\
    V(LAnd)

#define BINARY_SUB_ENUM(V)\
    case BinOp::k##V: stub = stubs()->GetBinary##V##Stub(); break;


    switch (op->subtype()) {
     BINARY_SUB_TYPES(BINARY_SUB_ENUM)
     default: emitb(0xcc); break;
    }
#undef BINARY_SUB_ENUM
#undef BINARY_SUB_TYPES

    // rax and rbx are already on stack
    // so just call stub
    if (stub != NULL) Call(stub);

    bind(&done);

    // Unwind stored rax and rbx
    addq(rsp, 16);
    ChangeAlign(-2);
  }

  Result(rax);
  Restore(rbx);
  Restore(rax);

  return node;
}

} // namespace internal
} // namespace candor
