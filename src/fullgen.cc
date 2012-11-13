#include "fullgen.h"
#include "fullgen-inl.h"
#include "heap.h"
#include "heap-inl.h"
#include "scope.h" // ScopeSlot
#include "macroassembler.h"

namespace candor {
namespace internal {

Fullgen::Fullgen(Heap* heap)
    : Visitor<FInstruction>(kPreorder),
      heap_(heap),
      root_(heap),
      instr_id_(-2),
      current_function_(NULL),
      loop_start_(NULL),
      loop_end_(NULL),
      source_map_(heap->source_map()) {
}


void Fullgen::Generate(AstNode* ast) {
  FFunction* fn = new FFunction(ast);
  fn->Init(this);

  work_queue_.Push(fn);
  while (work_queue_.length() > 0) {
    EmptySlots();

    FFunction* current = work_queue_.Shift();
    set_current_function(current);
    Visit(current->ast());
    set_current_function(NULL);
  }
}


FInstruction* Fullgen::Visit(AstNode* node) {
  FInstruction* res = Visitor<FInstruction>::Visit(node);
  if (res != NULL) res->ast(node);
  return res;
}


FInstruction* Fullgen::VisitFunction(AstNode* stmt) {
  if (current_function()->ast() == stmt) {
    current_function()->body = new FLabel();
    Add(current_function()->body);
    Add(new FEntry());

    VisitChildren(stmt);

    // In case if there're no return statement in function
    {
      FScopedSlot result(this);

      FInstruction* arg = Add(new FNil());
      arg->SetResult(&result);
      Add(new FReturn())->AddArg(&result);
    }
  } else {
    FFunction* fn = new FFunction(stmt);
    Add(fn);
    work_queue_.Push(fn);
  }
  return NULL;
}


FInstruction* Fullgen::VisitReturn(AstNode* node) {
  FScopedSlot result(this);
  Visit(node->lhs())->SetResult(&result);

  return Add(new FReturn())->AddArg(&result);
}


FInstruction* Fullgen::VisitLiteral(AstNode* node) {
  return Add(new FLiteral(node->type(), root_.Put(node)));
}


FInstruction* Fullgen::VisitNumber(AstNode* node) {
  return VisitLiteral(node);
}


FInstruction* Fullgen::VisitTrue(AstNode* node) {
  return VisitLiteral(node);
}


FInstruction* Fullgen::VisitFalse(AstNode* node) {
  return VisitLiteral(node);
}


FInstruction* Fullgen::VisitString(AstNode* node) {
  return VisitLiteral(node);
}


FInstruction* Fullgen::VisitProperty(AstNode* node) {
  return VisitLiteral(node);
}


FInstruction* Fullgen::VisitAssign(AstNode* stmt) {
  FScopedSlot rhs(this);
  FInstruction* irhs = Visit(stmt->rhs())->SetResult(&rhs);

  if (stmt->lhs()->is(AstNode::kValue)) {
    AstValue* value = AstValue::Cast(stmt->lhs());

    if (value->slot()->is_stack()) {
      // No instruction is needed
      Add(new FStore())->AddArg(CreateOperand(value->slot()))->AddArg(&rhs);
    } else {
      Add(new FStoreContext())
        ->AddArg(CreateOperand(value->slot()))
        ->AddArg(&rhs);
    }
  } else if (stmt->lhs()->is(AstNode::kMember)) {
    FScopedSlot property(this);
    FScopedSlot receiver(this);
    Visit(stmt->lhs()->rhs())->SetResult(&property);
    Visit(stmt->lhs()->lhs())->SetResult(&receiver);

    Add(new FStoreProperty())
        ->AddArg(&receiver)
        ->AddArg(&property)
        ->AddArg(&rhs);
  } else {
    UNEXPECTED
  }

  return irhs;
}


FInstruction* Fullgen::VisitValue(AstNode* node) {
  AstValue* value = AstValue::Cast(node);
  ScopeSlot* slot = value->slot();
  if (slot->is_stack()) {
    // External value
    return Add(new FLoad())->AddArg(CreateOperand(slot));
  } else {
    return Add(new FLoadContext())->AddArg(CreateOperand(slot));
  }
}


FInstruction* Fullgen::VisitMember(AstNode* node) {
  FScopedSlot prop(this);
  FScopedSlot recv(this);
  Visit(node->rhs())->SetResult(&prop);
  Visit(node->lhs())->SetResult(&recv);
  return Add(new FLoadProperty())->AddArg(&recv)->AddArg(&prop);
}


FInstruction* Fullgen::VisitDelete(AstNode* node) {
  FScopedSlot prop(this);
  FScopedSlot recv(this);
  Visit(node->lhs()->rhs())->SetResult(&prop);
  Visit(node->lhs()->lhs())->SetResult(&recv);
  return Add(new FDeleteProperty())->AddArg(&recv)->AddArg(&prop);
}


FInstruction* Fullgen::VisitObjectLiteral(AstNode* node) {
  ObjectLiteral* obj = ObjectLiteral::Cast(node);
  FScopedSlot slot(this);
  FInstruction* res = Add(new FAllocateObject(obj->keys()->length()));
  res->SetResult(&slot);

  AstList::Item* khead = obj->keys()->head();
  AstList::Item* vhead = obj->values()->head();
  for (; khead != NULL; khead = khead->next(), vhead = vhead->next()) {
    FScopedSlot value(this);
    FScopedSlot key(this);
    Visit(vhead->value())->SetResult(&value);
    Visit(khead->value())->SetResult(&key);

    Add(new FStoreProperty())
        ->AddArg(&slot)
        ->AddArg(&key)
        ->AddArg(&value);
  }

  return Add(new FChi())->AddArg(&slot);
}


FInstruction* Fullgen::VisitArrayLiteral(AstNode* node) {
  FScopedSlot slot(this);
  FInstruction* res = Add(new FAllocateArray(node->children()->length()));
  res->SetResult(&slot);

  AstList::Item* head = node->children()->head();
  for (uint64_t i = 0; head != NULL; head = head->next(), i++) {
    FScopedSlot key(this);
    FScopedSlot value(this);
    GetNumber(i)->SetResult(&key);
    Visit(head->value())->SetResult(&value);

    Add(new FStoreProperty())
        ->AddArg(&slot)
        ->AddArg(&key)
        ->AddArg(&value);
  }

  return Add(new FChi())->AddArg(&slot);
}


FInstruction* Fullgen::VisitNil(AstNode* node) {
  return Add(new FNil());
}


FInstruction* Fullgen::VisitIf(AstNode* node) {
  FScopedSlot cond(this);
  Visit(node->lhs())->SetResult(&cond);

  FLabel* t = new FLabel();
  FLabel* f = new FLabel();
  FLabel* join = new FLabel();
  Add(new FIf(t, f))->AddArg(&cond);

  Add(t);
  Visit(node->rhs());

  Add(new FGoto(join));
  Add(f);
  AstList::Item* else_branch = node->children()->head()->next()->next();
  if (else_branch != NULL) Visit(else_branch->value());

  Add(join);

  return NULL;
}


FInstruction* Fullgen::VisitWhile(AstNode* node) {
  FScopedSlot cond(this);
  Visit(node->lhs())->SetResult(&cond);

  FLabel* prev_start = loop_start_;
  FLabel* prev_end = loop_end_;

  loop_start_ = new FLabel();
  FLabel* body = new FLabel();
  loop_end_ = new FLabel();

  Add(loop_start_);
  Add(new FIf(body, loop_end_))->AddArg(&cond);

  Add(body);
  Visit(node->rhs());

  // Loop
  Add(new FGoto(loop_start_));

  Add(loop_end_);

  // Restore
  loop_start_ = prev_start;
  loop_end_ = prev_end;

  return NULL;
}


FInstruction* Fullgen::VisitBreak(AstNode* node) {
  assert(loop_end_ != NULL);
  return Add(new FBreak(loop_end_));
}


FInstruction* Fullgen::VisitContinue(AstNode* node) {
  assert(loop_start_ != NULL);
  return Add(new FContinue(loop_start_));
}


FInstruction* Fullgen::VisitCall(AstNode* stmt) {
  return NULL;
}


FInstruction* Fullgen::VisitClone(AstNode* node) {
  FScopedSlot lhs(this);
  Visit(node->lhs())->SetResult(&lhs);
  return Add(new FClone())->AddArg(&lhs);
}


FInstruction* Fullgen::VisitTypeof(AstNode* node) {
  FScopedSlot lhs(this);
  Visit(node->lhs())->SetResult(&lhs);
  return Add(new FTypeof())->AddArg(&lhs);
}


FInstruction* Fullgen::VisitSizeof(AstNode* node) {
  FScopedSlot lhs(this);
  Visit(node->lhs())->SetResult(&lhs);
  return Add(new FSizeof())->AddArg(&lhs);
}


FInstruction* Fullgen::VisitKeysof(AstNode* node) {
  FScopedSlot lhs(this);
  Visit(node->lhs())->SetResult(&lhs);
  return Add(new FKeysof())->AddArg(&lhs);
}


FInstruction* Fullgen::VisitUnOp(AstNode* node) {
  UnOp* op = UnOp::Cast(node);
  BinOp::BinOpType type;

  if (op->is_changing()) {
    FInstruction* res = NULL;
    FInstruction* load = NULL;
    FInstruction* add = NULL;
    FScopedSlot value(this);
    FScopedSlot load_slot(this);
    FScopedSlot one(this);

    // ++i, i++
    type = (op->subtype() == UnOp::kPreInc || op->subtype() == UnOp::kPostInc) ?
          BinOp::kAdd : BinOp::kSub;

    load = Visit(op->lhs())->SetResult(&load_slot);
    GetNumber(1)->SetResult(&one);

    add = Add(new FBinOp(type));
    add->AddArg(&load_slot)->AddArg(&one)->SetResult(&value);

    if (op->subtype() == UnOp::kPreInc || op->subtype() == UnOp::kPreDec) {
      res = add;
    } else {
      res = load;
    }

    // Assign new value to variable
    if (op->lhs()->is(AstNode::kValue)) {
      ScopeSlot* slot = AstValue::Cast(op->lhs())->slot();

      if (slot->is_stack()) {
        Add(new FStore())->AddArg(CreateOperand(slot))->AddArg(&value);
      } else {
        Add(new FStoreContext())
            ->AddArg(CreateOperand(slot))
            ->AddArg(&value);
      }
    } else if (op->lhs()->is(AstNode::kMember)) {
      FOperand* receiver = load->inputs[0];
      FOperand* property = load->inputs[1];

      Add(new FStoreProperty())
          ->AddArg(receiver)
          ->AddArg(property)
          ->AddArg(&value);
    } else {
      UNEXPECTED
    }

    return Add(new FChi())->AddArg(res->result);
  } else if (op->subtype() == UnOp::kPlus || op->subtype() == UnOp::kMinus) {
    // +i = 0 + i,
    // -i = 0 - i
    AstNode* zero = new AstNode(AstNode::kNumber, node);
    zero->value("0");
    zero->length(1);

    type = op->subtype() == UnOp::kPlus ? BinOp::kAdd : BinOp::kSub;

    AstNode* wrap = new BinOp(type, zero, op->lhs());

    return Visit(wrap);
  } else if (op->subtype() == UnOp::kNot) {
    FScopedSlot lhs(this);
    Visit(op->lhs())->SetResult(&lhs);
    return Add(new FNot())->AddArg(&lhs);
  } else {
    UNEXPECTED
  }
}


FInstruction* Fullgen::VisitBinOp(AstNode* node) {
  BinOp* op = BinOp::Cast(node);
  FScopedSlot lhs(this);
  Visit(node->lhs())->SetResult(&lhs);

  FScopedSlot rhs(this);
  Visit(node->rhs())->SetResult(&rhs);

  return Add(new FBinOp(op->subtype()))->AddArg(&lhs)->AddArg(&rhs);
}


void Fullgen::Print(PrintBuffer* p) {
  ZoneList<FInstruction*>::Item* ihead = instructions_.head();
  for (; ihead != NULL; ihead = ihead->next()) {
    ihead->value()->Print(p);
  }
}


void FChi::Generate(Masm* masm) {
  // Just move input to output
}


FScopedSlot::FScopedSlot(Fullgen* f) : f_(f) {
  operand_ = f->GetSlot();
}


FScopedSlot::~FScopedSlot() {
  f_->ReleaseSlot(operand_);
  operand_ = NULL;
}


void FOperand::Print(PrintBuffer* p) {
  switch (type_) {
   case kStack: p->Print("s[%d]", index_); break;
   case kContext: p->Print("c[%d:%d]", index_, depth_); break;
   case kRegister: p->Print("%s", RegisterNameByIndex(index_)); break;
   default: UNEXPECTED
  }
}

} // namespace internal
} // namespace candor
