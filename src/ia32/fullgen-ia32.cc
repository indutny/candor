#include "fullgen.h"
#include "macroassembler.h"
#include "code-space.h" // CodeSpace
#include "heap.h" // Heap
#include "heap-inl.h"
#include "source-map.h" // SourceMap
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
        RelocationInfo::kLong,
        offset - 4);
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


Fullgen::Fullgen(CodeSpace* space, SourceMap* map) : Masm(space),
                                                     Visitor(kPreorder),
                                                     space_(space),
                                                     visitor_type_(kValue),
                                                     current_function_(NULL),
                                                     loop_start_(NULL),
                                                     loop_end_(NULL),
                                                     source_map_(map),
                                                     error_msg_(NULL),
                                                     error_pos_(0) {
  InitRoots();
}


void Fullgen::InitRoots() {
  // Create a `global` object
  root_context()->Push(HObject::NewEmpty(heap()));

  // Place some root values
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
  if (fn()->offset() != -1) {
    fullgen()->source_map()->Push(fullgen()->offset(), fn()->offset());
  }

  // Generate function's body
  fullgen()->GeneratePrologue(fn());
  fullgen()->VisitChildren(fn());

  // In case if function doesn't have `return` statements
  // we should still return `nil` value
  masm()->movl(eax, Immediate(Heap::kTagNil));

  fullgen()->GenerateEpilogue(fn());
  fullgen()->FinalizeSpills();
}


void Fullgen::Throw(Heap::Error err) {
  assert(current_node() != NULL);
  SetError(Heap::ErrorToString(err), current_node()->offset());
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
  // edx <- root address
  // edi <- reference to parent context (zero for main)
  // esi <- unboxed arguments count (tagged)
  push(ebp);
  movl(ebp, esp);

  // Allocate space for spill slots and on-stack variables
  // and 2 slots for root and context
  AllocateSpills(stmt->stack_slots() + 6);

  FillStackSlots();

  // Save root
  movl(root_op, edx);

  // Allocate context and clear stack slots
  AllocateContext(stmt->context_slots());
  movl(context_op, edi);

  // Place all arguments into their slots
  Label body(this);

  FunctionLiteral* fn = FunctionLiteral::Cast(stmt);
  AstList::Item* item = fn->args()->head();
  uint32_t i = 0;
  uint32_t argc = fn->args()->length();

  // scratch <- index
  movl(scratch, Immediate(HNumber::Tag(0)));

  // ecx <- pointer to current stack slot
  movl(ecx, ebp);
  // skip return address and previous rbp
  addl(ecx, Immediate(2 * 4));
  while (item != NULL) {
    Operand lhs(eax, 0);

    cmpl(esi, scratch);
    jmp(kLt, &body);

    switch (item->value()->type()) {
     case AstNode::kValue:
      {
        AstValue* arg = AstValue::Cast(item->value());

        if (arg->slot()->use_count() > 1) {
          Operand rhs(ecx, 0);
          VisitFor(kSlot, arg);

          movl(edx, rhs);
          movl(slot(), edx);
        }
      }
      break;
     case AstNode::kVarArg:
      {
        AstValue* arg = AstValue::Cast(item->value()->lhs());
        Spill scratch_s(this, scratch);

        // Get left arguments count
        // = ( total - current real - expected leading )
        movl(edx, esi);
        subl(edx, scratch);
        if (argc != i + 1) {
          subl(edx, Immediate(HNumber::Tag(argc - i - 1)));
        }
        shl(edx, Immediate(2));

        if (arg->slot()->use_count() > 1) {
          // Interior pointer to the arguments
          movl(eax, ecx);
        }

        // Move stack pointer forward by vararg's length
        movl(scratch, edx);
        addl(ecx, edx);
        subl(ecx, Immediate(4));

        if (arg->slot()->use_count() > 1) {
          Spill ecx_s(this, ecx);

          // Call stub to generate var arg array
          Call(stubs()->GetVarArgStub());

          ecx_s.Unspill();
          movl(edx, eax);

          VisitFor(kSlot, arg);
          movl(slot(), edx);
        }

        scratch_s.Unspill();
      }
      break;
     default:
      UNEXPECTED
      break;
    }

    // Advance
    addl(scratch, Immediate(HNumber::Tag(1)));
    addl(ecx, Immediate(4));
    i++;
    item = item->next();
  }

  bind(&body);

  // Cleanup junk
  xorl(eax, eax);
  xorl(ecx, ecx);
  xorl(edx, edx);
  xorl(scratch, scratch);
  xorl(esi, esi);
  xorl(edi, edi);
}


void Fullgen::GenerateEpilogue(AstNode* stmt) {
  // eax will hold result of function
  movl(esp, ebp);
  pop(ebp);

  ret(0);
}


void Fullgen::PlaceInRoot(char* addr) {
  Operand root_slot(eax, HContext::GetIndexDisp(root_context()->length()));
  movl(eax, root_op);
  movl(eax, root_slot);

  root_context()->Push(addr);
}


char* Fullgen::AllocateRoot() {
  return HContext::New(heap(), root_context());
}


AstNode* Fullgen::Visit(AstNode* node) {
  // Amend source_map
  if (node->offset() != -1) {
    source_map()->Push(offset(), node->offset());
  }

  return Visitor::Visit(node);
}


AstNode* Fullgen::VisitFor(VisitorType type, AstNode* node) {
  // Save previous data
  VisitorType stored_type = visitor_type_;

  // Set new
  visitor_type_ = type;

  // Visit node
  AstNode* result = Visit(node);

  // Restore
  visitor_type_ = stored_type;

  return result;
}


AstNode* Fullgen::VisitFunction(AstNode* stmt) {
  FunctionLiteral* fn = FunctionLiteral::Cast(stmt);
  FFunction* ffn = new CandorFunction(this, fn);
  fns_.Push(ffn);

  movl(ecx, Immediate(0));
  ffn->Use(offset());

  // Allocate function object that'll reference to current scope
  // and have address of actual code

  if (fn->variable() != NULL) {
    AllocateFunction(ecx, edx, fn->args()->length());

    Spill edx_s(this, edx);

    AstNode* assign = new AstNode(AstNode::kAssign, stmt);
    assign->children()->Push(fn->variable());
    assign->children()->Push(new FAstSpill(&edx_s));

    Visit(assign);
  } else {
    AllocateFunction(ecx, eax, fn->args()->length());
  }

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

  Label not_function(this), incorrect_vararg(this), done(this);

  AstNode* name = AstValue::Cast(fn->variable())->name();

  // handle __$gc() call
  if (fn->variable()->is(AstNode::kValue)) {
    if (name->length() == 5 && strncmp(name->value(), "__$gc", 5) == 0) {
      Call(stubs()->GetCollectGarbageStub());
      movl(eax, Immediate(Heap::kTagNil));
      return stmt;
    } else if (name->length() == 8 &&
               strncmp(name->value(), "__$trace", 8) == 0) {
      uint32_t ip = offset();

      // Pass ip
      movl(eax, Immediate(0));
      RelocationInfo* r = new RelocationInfo(RelocationInfo::kAbsolute,
                                             RelocationInfo::kLong,
                                             offset() - 4);
      relocation_info_.Push(r);
      r->target(ip);
      Call(stubs()->GetStackTraceStub());
      return stmt;
    }
  }

  Spill receiver_s(this);
  if (fn->args()->length() > 0 &&
      fn->args()->head()->value()->is(AstNode::kSelf)) {
    assert(fn->variable()->is(AstNode::kMember));

    AstNode* receiver = fn->variable()->lhs();
    AstNode* property = fn->variable()->rhs();

    VisitFor(kValue, receiver);
    receiver_s.SpillReg(eax);

    AstNode* member = new AstNode(AstNode::kMember, receiver);
    member->children()->Push(new FAstSpill(&receiver_s));
    member->children()->Push(property);

    VisitFor(kValue, member);
  } else {
    VisitFor(kValue, fn->variable());
  }

  Spill eax_s(this, eax);

  IsUnboxed(eax, NULL, &not_function);
  IsNil(eax, NULL, &not_function);
  IsHeapObject(Heap::kTagFunction, eax, &not_function, NULL);

  Spill esi_s(this, esi), edi_s(this, edi);

  Spill stack_s(this, esp);
  {
    movl(eax, Immediate(Heap::kTagNil));
    Spill vararg(this);
    uint32_t argc = fn->args()->length();
    uint32_t i;

    if (argc > 0 && fn->args()->tail()->value()->is(AstNode::kVarArg)) {
      argc--;
      VisitFor(kValue, fn->args()->tail()->value());

      IsUnboxed(eax, NULL, &incorrect_vararg);
      IsNil(eax, NULL, &incorrect_vararg);
      IsHeapObject(Heap::kTagArray, eax, &incorrect_vararg, NULL);

      vararg.SpillReg(eax);
    }

    // Set argc
    movl(esi, Immediate(HNumber::Tag(argc)));

    // Allocate space on stack

    // For vararg (and increase esi)
    if (!vararg.is_empty()) {
      AllocateVarArgSlots(&vararg, esi);
    }

    // And for regular arguments
    for (i = 0; i < argc; i++) {
      push(eax);
    }

    // Align stack if needed
    switch (argc & 3) {
     case 1: push(eax);
     case 2: push(eax);
     case 3: push(eax);
     default: break;
    }

    AstList::Item* item = fn->args()->head();

    movl(ebx, esp);
    Spill ebx_s(this, ebx);

    // Put arguments
    // [top] [1] ... [n]
    i = 0;
    while (item != NULL) {
      Operand arg_slot(ebx, i++ * 4);

      if (item->value()->is(AstNode::kSelf)) {
        receiver_s.Unspill(eax);
      } else {
        VisitFor(kValue, item->value());
      }

      ebx_s.Unspill();
      movl(arg_slot, eax);

      item = item->next();
    }

    // Put vararg
    if (!vararg.is_empty()) {
      vararg.Unspill(eax);
      ebx_s.Unspill();
      addl(ebx, Immediate((i - 1) * 4));

      Call(stubs()->GetPutVarArgStub());
    }

    // Generate calling code
    eax_s.Unspill();
    CallFunction(eax);
  }

  // Unwind stack
  stack_s.Unspill();

  edi_s.Unspill();
  esi_s.Unspill();

  jmp(&done);
  bind(&incorrect_vararg);

  movl(eax, Immediate(Heap::kTagNil));

  jmp(&done);
  bind(&not_function);

  movl(eax, Immediate(Heap::kTagNil));

  bind(&done);

  return stmt;
}


AstNode* Fullgen::VisitAssign(AstNode* stmt) {
  Label done(this);

  // Get value of right-hand side expression in ebx
  VisitFor(kValue, stmt->rhs());
  Spill eax_s(this, eax);

  // Get target slot for left-hand side
  VisitFor(kSlot, stmt->lhs());

  eax_s.Unspill(scratch);

  // If slot's base is nil - just return value
  if (!slot().base().is(ebp)) {
    IsNil(slot().base(), NULL, &done);
  }

  // Put value into slot
  movl(slot(), scratch);

  // Propagate result of assign operation
  bind(&done);
  movl(eax, scratch);

  return stmt;
}


AstNode* Fullgen::VisitValue(AstNode* node) {
  AstValue* value = AstValue::Cast(node);

  // If it's Fullgen generated AST Value
  if (value->is_spill()) {
    assert(visiting_for_value());
    FAstSpill::Cast(value)->spill()->Unspill(eax);
    return node;
  }

  if (value->is_operand()) {
    slot().base(FAstOperand::Cast(value)->op()->base());
    slot().disp(FAstOperand::Cast(value)->op()->disp());

    if (visiting_for_value()) {
      movl(eax, slot());
    }

    return node;
  }

  slot().scale(Operand::one);

  // Get pointer to slot first
  if (value->slot()->is_stack()) {
    // On stack variables
    slot().base(ebp);
    slot().disp(-4 * (value->slot()->index() + 3));
  } else {
    int32_t depth = value->slot()->depth();

    if (depth == -2) {
      // Root register lookup
      movl(eax, root_op);
      slot().base(eax);
      slot().disp(4 * (value->slot()->index() + 3));
    } else if (depth == -1) {
      // Global lookup
      movl(eax, root_op);
      slot().base(eax);
      slot().disp(HContext::GetIndexDisp(Heap::kRootGlobalIndex));

      if (visiting_for_slot()) {
        Throw(Heap::kErrorIncorrectLhs);
      }
    } else {
      // Context variables
      movl(eax, context_op);

      // Lookup context
      while (--depth >= 0) {
        Operand parent(eax, HContext::kParentOffset);
        movl(eax, parent);
      }

      slot().base(eax);
      slot().scale(Operand::one);
      // Skip tag, code addr and reference to parent scope
      slot().disp(HContext::GetIndexDisp(value->slot()->index()));
    }
  }

  // If we was asked to return value - dereference slot
  if (visiting_for_value()) {
    movl(eax, slot());
  }

  return node;
}


AstNode* Fullgen::VisitMember(AstNode* node) {
  VisitFor(kValue, node->lhs());
  Spill eax_s(this, eax);

  VisitFor(kValue, node->rhs());
  movl(edx, eax);
  eax_s.Unspill(eax);

  movl(ecx, Immediate(visiting_for_slot()));
  Call(stubs()->GetLookupPropertyStub());

  // Make eax look like unboxed number to GC
  dec(eax);
  CheckGC();
  inc(eax);

  Label done(this);

  IsNil(eax, NULL, &done);

  eax_s.Unspill(ebx);

  Operand qmap(ebx, HObject::kMapOffset);
  movl(ebx, qmap);
  addl(eax, ebx);

  slot().base(eax);
  slot().disp(0);

  // Unbox value if asked
  if (visiting_for_value()) {
    movl(eax, slot());
  }

  bind(&done);

  slot().base(eax);
  slot().disp(0);

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
    int32_t value = StringToInt(node->value(), node->length());
    movl(eax, Immediate(HNumber::Tag(value)));
  }

  return node;
}


AstNode* Fullgen::VisitString(AstNode* node) {
  if (visiting_for_slot()) {
    Throw(Heap::kErrorIncorrectLhs);
    return node;
  }

  // Unescape string
  uint32_t length;
  const char* unescaped = Unescape(node->value(), node->length(), &length);

  PlaceInRoot(HString::New(heap(), Heap::kTenureOld, unescaped, length));

  delete unescaped;

  return node;
}


AstNode* Fullgen::VisitProperty(AstNode* node) {
  // kProperty is essentially the same as string
  return VisitString(node);
}


AstNode* Fullgen::VisitIf(AstNode* node) {
  Label fail_body(this), done(this);

  AstNode* expr = node->lhs();
  AstNode* success = node->rhs();
  AstList::Item* fail_item = node->children()->head()->next()->next();
  AstNode* fail = NULL;
  if (fail_item != NULL) fail = fail_item->value();

  VisitFor(kValue, expr);

  Call(stubs()->GetCoerceToBooleanStub());

  IsTrue(eax, &fail_body, NULL);

  VisitFor(kValue, success);

  jmp(&done);
  bind(&fail_body);

  if (fail != NULL) VisitFor(kValue, fail);

  bind(&done);

  return node;
}


AstNode* Fullgen::VisitWhile(AstNode* node) {
  Label loop_start(this), loop_end(this);

  AstNode* expr = node->lhs();
  AstNode* body = node->rhs();

  LoopVisitor visitor(this, &loop_start, &loop_end);

  bind(&loop_start);

  VisitFor(kValue, expr);

  Call(stubs()->GetCoerceToBooleanStub());

  IsTrue(eax, &loop_end, NULL);

  VisitFor(kValue, body);

  jmp(&loop_start);

  bind(&loop_end);

  return node;
}


AstNode* Fullgen::VisitNil(AstNode* node) {
  if (visiting_for_slot()) {
    Throw(Heap::kErrorIncorrectLhs);
    return node;
  }

  movl(eax, Immediate(Heap::kTagNil));

  return node;
}


AstNode* Fullgen::VisitTrue(AstNode* node) {
  if (visiting_for_slot()) {
    Throw(Heap::kErrorIncorrectLhs);
    return node;
  }

  Operand true_slot(eax, HContext::GetIndexDisp(Heap::kRootTrueIndex));
  movl(eax, root_op);
  movl(eax, true_slot);

  return node;
}


AstNode* Fullgen::VisitFalse(AstNode* node) {
  if (visiting_for_slot()) {
    Throw(Heap::kErrorIncorrectLhs);
    return node;
  }

  Operand false_slot(eax, HContext::GetIndexDisp(Heap::kRootFalseIndex));
  movl(eax, root_op);
  movl(eax, false_slot);

  return node;
}


AstNode* Fullgen::VisitObjectLiteral(AstNode* node) {
  if (visiting_for_slot()) {
    Throw(Heap::kErrorIncorrectLhs);
    return node;
  }

  ObjectLiteral* obj = ObjectLiteral::Cast(node);

  // Ensure that map will be filled only by half at maximum
  movl(eax,
       Immediate(HNumber::Tag(PowerOfTwo(node->children()->length() << 1))));
  AllocateObjectLiteral(Heap::kTagObject, eax, edx);

  Spill edx_s(this, edx);

  // Set every key/value pair
  assert(obj->keys()->length() == obj->values()->length());
  AstList::Item* key = obj->keys()->head();
  AstList::Item* value = obj->values()->head();
  while (key != NULL) {
    AstNode* member = new AstNode(AstNode::kMember, node);
    member->children()->Push(new FAstSpill(&edx_s));
    member->children()->Push(key->value());

    AstNode* assign = new AstNode(AstNode::kAssign, node);
    assign->children()->Push(member);
    assign->children()->Push(value->value());

    VisitFor(kValue, assign);

    key = key->next();
    value = value->next();
  }

  edx_s.Unspill(eax);

  return node;
}


AstNode* Fullgen::VisitArrayLiteral(AstNode* node) {
  if (visiting_for_slot()) {
    Throw(Heap::kErrorIncorrectLhs);
    return node;
  }

  // Ensure that map will be filled only by half at maximum
  movl(eax,
       Immediate(HNumber::Tag(PowerOfTwo(node->children()->length() << 1))));
  AllocateObjectLiteral(Heap::kTagArray, eax, edx);

  Spill edx_s(this, edx);

  AstList::Item* item = node->children()->head();
  uint32_t index = 0;
  while (item != NULL) {
    char keystr[32];
    AstNode* key = new AstNode(AstNode::kNumber, node);
    key->value(keystr);
    key->length(snprintf(keystr, sizeof(keystr), "%u", index));

    AstNode* member = new AstNode(AstNode::kMember, node);
    member->children()->Push(new FAstSpill(&edx_s));
    member->children()->Push(key);

    AstNode* assign = new AstNode(AstNode::kAssign, node);
    assign->children()->Push(member);
    assign->children()->Push(item->value());

    VisitFor(kValue, assign);

    item = item->next();
    index++;
  }

  edx_s.Unspill(eax);

  return node;
}


AstNode* Fullgen::VisitReturn(AstNode* node) {
  if (node->lhs() != NULL) {
    // Get value of expression
    VisitFor(kValue, node->lhs());
  } else {
    // Or just nullify output
    movl(eax, Immediate(Heap::kTagNil));
  }

  GenerateEpilogue(current_function()->fn());

  return node;
}


AstNode* Fullgen::VisitClone(AstNode* node) {
  VisitFor(kValue, node->lhs());
  Call(stubs()->GetCloneObjectStub());

  return node;
}


AstNode* Fullgen::VisitDelete(AstNode* node) {
  if (!node->lhs()->is(AstNode::kMember)) {
    Throw(Heap::kErrorIncorrectLhs);
    return node;
  }

  AstNode* receiver = node->lhs()->lhs();
  AstNode* property = node->lhs()->rhs();

  VisitFor(kValue, property);
  Spill eax_s(this, eax);

  VisitFor(kValue, receiver);
  eax_s.Unspill(ebx);

  Call(stubs()->GetDeletePropertyStub());

  return node;
}


AstNode* Fullgen::VisitBreak(AstNode* node) {
  if (loop_end() == NULL) {
    Throw(Heap::kErrorExpectedLoop);
    return node;
  }

  jmp(loop_end());

  return node;
}


AstNode* Fullgen::VisitContinue(AstNode* node) {
  if (loop_start() == NULL) {
    Throw(Heap::kErrorExpectedLoop);
    return node;
  }

  jmp(loop_start());

  return node;
}


AstNode* Fullgen::VisitTypeof(AstNode* node) {
  if (visiting_for_slot()) {
    Throw(Heap::kErrorIncorrectLhs);
    return node;
  }

  Align a(this);

  VisitFor(kValue, node->lhs());
  movl(edx, root_op);
  Call(stubs()->GetTypeofStub());

  return node;
}


AstNode* Fullgen::VisitSizeof(AstNode* node) {
  if (visiting_for_slot()) {
    Throw(Heap::kErrorIncorrectLhs);
    return node;
  }

  Align a(this);

  VisitFor(kValue, node->lhs());
  Call(stubs()->GetSizeofStub());

  return node;
}


AstNode* Fullgen::VisitKeysof(AstNode* node) {
  if (visiting_for_slot()) {
    Throw(Heap::kErrorIncorrectLhs);
    return node;
  }

  Align a(this);

  VisitFor(kValue, node->lhs());
  Call(stubs()->GetKeysofStub());

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
    AstNode* one = new AstNode(AstNode::kNumber, node);
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

    AstNode* assign = new AstNode(AstNode::kAssign, node);
    assign->children()->Push(op->lhs());
    assign->children()->Push(rhs);

    // ++a => a = a + 1
    if (op->subtype() == UnOp::kPreInc || op->subtype() == UnOp::kPreDec) {
      Visit(assign);
      return node;
    }

    // a++ => $scratch = a; a = $scratch + 1; $scratch
    VisitFor(kSlot, op->lhs());

    Label done(this), nil_result(this);

    // If slot's base is nil - just return value
    IsNil(slot().base(), NULL, &nil_result);

    // Get value
    movl(scratch, slot());
    Spill scratch_s(this, scratch);

    // Put slot's value into ebx
    movl(ebx, slot());

    Spill ebx_s(this, ebx);

    assign->children()->head()->value(new FAstOperand(&slot()));
    rhs->children()->head()->value(new FAstSpill(&ebx_s));
    VisitFor(kValue, assign);

    scratch_s.Unspill(eax);

    jmp(&done);

    bind(&nil_result);
    movl(eax, slot().base());

    bind(&done);

  } else if (op->subtype() == UnOp::kPlus || op->subtype() == UnOp::kMinus) {
    // +a = 0 + a
    // -a = 0 - a
    // TODO: Parser should genereate negative numbers where possible

    AstNode* zero = new AstNode(AstNode::kNumber, node);
    zero->value("0");
    zero->length(1);

    AstNode* wrap = new BinOp(
        op->subtype() == UnOp::kPlus ? BinOp::kAdd : BinOp::kSub,
        zero,
        op->lhs());

    VisitFor(kValue, wrap);

  } else if (op->subtype() == UnOp::kNot) {
    // Get value and convert it to boolean
    VisitFor(kValue, op->lhs());
    Call(stubs()->GetCoerceToBooleanStub());

    Label done(this), ret_false(this);

    Operand truev(edx, HContext::GetIndexDisp(Heap::kRootTrueIndex));
    Operand falsev(edx, HContext::GetIndexDisp(Heap::kRootFalseIndex));

    // Negate it
    movl(edx, root_op);
    IsTrue(eax, NULL, &ret_false);

    movl(eax, truev);

    jmp(&done);
    bind(&ret_false);

    movl(eax, falsev);

    bind(&done);

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

  VisitFor(kValue, op->lhs());

  Label call_stub(this), done(this);

  AstNode* rhs = op->rhs();

  // Fast case : unboxed +/- const
  if (rhs->is(AstNode::kNumber) &&
      !StringIsDouble(rhs->value(), rhs->length()) &&
      (op->subtype() == BinOp::kAdd || op->subtype() == BinOp::kSub)) {

    IsUnboxed(eax, &call_stub, NULL);

    int32_t num = HNumber::Tag(StringToInt(rhs->value(), rhs->length()));

    // addq and subq supports only long immediate (not quad)
    if (num >= -0x7fffffff && num <= 0x7fffffff) {
      switch (op->subtype()) {
       case BinOp::kAdd: addl(eax, Immediate(num)); break;
       case BinOp::kSub: subl(eax, Immediate(num)); break;
       default:
        UNEXPECTED
        break;
      }

      jmp(kNoOverflow, &done);

      // Restore on overflow
      switch (op->subtype()) {
       case BinOp::kAdd: subl(eax, Immediate(num)); break;
       case BinOp::kSub: addl(eax, Immediate(num)); break;
       default:
        UNEXPECTED
        break;
      }
    }
  }

  char* stub = NULL;

#define BINARY_SUB_TYPES(V)\
    V(Add)\
    V(Sub)\
    V(Mul)\
    V(Div)\
    V(Mod)\
    V(BAnd)\
    V(BOr)\
    V(BXor)\
    V(Shl)\
    V(Shr)\
    V(UShr)\
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

  assert(stub != NULL);

  bind(&call_stub);

  Spill eax_s(this, eax);
  VisitFor(kValue, op->rhs());
  movl(ebx, eax);
  eax_s.Unspill(eax);

  movl(edx, root_op);
  Call(stub);

  bind(&done);

  return node;
}

} // namespace internal
} // namespace candor
