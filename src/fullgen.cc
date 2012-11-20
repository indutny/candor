/**
 * Copyright (c) 2012, Fedor Indutny.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "fullgen.h"
#include "fullgen-inl.h"
#include "fullgen-instructions.h"
#include "fullgen-instructions-inl.h"
#include "heap.h"
#include "heap-inl.h"
#include "scope.h"  // ScopeSlot
#include "macroassembler.h"

namespace candor {
namespace internal {

bool Fullgen::log_ = false;

Fullgen::Fullgen(Heap* heap, Root* root, const char* filename)
    : Visitor<FInstruction>(kPreorder),
      heap_(heap),
      root_(root),
      filename_(filename),
      instr_id_(-2),
      current_function_(NULL),
      loop_start_(NULL),
      loop_end_(NULL),
      source_map_(heap->source_map()) {
}


void Fullgen::EnableLogging() {
  log_ = true;
}


void Fullgen::DisableLogging() {
  log_ = false;
}


void Fullgen::Build(AstNode* ast) {
  EmptySlots();

  FFunction* current = new FFunction(ast, 0);
  current->Init(this);
  set_current_function(current);
  Visit(current->root_ast());
  set_current_function(NULL);

  current->entry->stack_slots(stack_index_ +
                              current->root_ast()->stack_slots());
  Add(new FAlignCode());

  if (log_) {
    PrintBuffer p(stdout);
    p.Print("## Fullgen %s Start ##\n",
            filename_ == NULL ? "unknown" : filename_);
    Print(&p);
    p.Print("## Fulglen End ##\n");
  }
}


void Fullgen::Generate(Masm* masm) {
  FInstructionList::Item* ihead = instructions_.head();
  for (; ihead != NULL; ihead = ihead->next()) {
    FInstruction* instr = ihead->value();
    if (instr->type() == FInstruction::kEntry) {
      if (ihead->prev() != NULL) masm->FinalizeSpills();

      // +1 for argc
      masm->stack_slots(FEntry::Cast(instr)->stack_slots() + 1);
    }

    // Amend source map
    if (instr->ast() != NULL && instr->ast()->offset() >= 0) {
      source_map()->Push(masm->offset(), instr->ast()->offset());
    }

    // generate instruction itself
    instr->Generate(masm);
  }
  masm->FinalizeSpills();
  masm->AlignCode();
}


FInstruction* Fullgen::Visit(AstNode* node) {
  FInstruction* res = Visitor<FInstruction>::Visit(node);
  if (res != NULL) res->ast(node);
  return res;
}


void Fullgen::VisitChildren(AstNode* node) {
  AstList::Item* child = node->children()->head();
  for (; child != NULL; child = child->next()) {
    FInstruction* res = Visit(child->value());

    // Always set result
    if (res != NULL && res->result == NULL) {
      FScopedSlot slot(this);
      res->SetResult(&slot);
    }
  }
}


void Fullgen::LoadArguments(FunctionLiteral* fn) {
  FScopedSlot index(this);
  FScopedSlot arg_slot(this);
  int flat_index = 0;
  bool seen_varg = false;

  if (fn->args()->length() > 0) {
    Add(GetNumber(0))->SetResult(&index);
  }

  AstList::Item* args_head = fn->args()->head();
  for (int i = 0; args_head != NULL; args_head = args_head->next(), i++) {
    AstNode* arg = args_head->value();
    bool varg = false;

    FInstruction* instr;
    if (arg->is(AstNode::kVarArg)) {
      assert(arg->lhs()->is(AstNode::kValue));
      arg = arg->lhs();

      varg = true;
      seen_varg = true;
      instr = new FLoadVarArg();
    } else {
      instr = new FLoadArg();
    }

    AstValue* value = AstValue::Cast(arg);

    FInstruction* varg_rest = NULL;
    FInstruction* varg_arr = NULL;
    if (varg) {
      // Result vararg array
      varg_arr = Add(new FAllocateArray(HArray::kVarArgLength));

      // Add number of arguments that are following varg
      varg_rest = GetNumber(fn->args()->length() - i - 1);
    }
    FInstruction* load_arg = Add(instr)->AddArg(&index)->SetResult(&arg_slot);

    if (varg) {
      FScopedSlot rest(this), arr(this);
      varg_rest->SetResult(&rest);
      varg_arr->SetResult(&arr);

      load_arg->AddArg(&rest)->AddArg(&arr);
      load_arg = varg_arr;
    }

    if (value->slot()->is_stack()) {
      Add(new FStore())
          ->AddArg(CreateOperand(value->slot()))
          ->AddArg(load_arg->result);
    } else {
      Add(new FStoreContext())
          ->AddArg(CreateOperand(value->slot()))
          ->AddArg(load_arg->result);
    }

    // Do not generate index if args has ended
    if (args_head->next() == NULL) continue;

    // Increment index
    if (!varg) {
      // By 1
      if (!seen_varg) {
        // Index is linear here, just generate new literal
        GetNumber(++flat_index)->SetResult(&index);
      } else {
        // Do "Full" math
        AstNode* one = new AstNode(AstNode::kNumber, fn);

        one->value("1");
        one->length(1);

        FScopedSlot f_one(this);
        Visit(one)->SetResult(&f_one);
        Add(new FBinOp(BinOp::kAdd))
            ->AddArg(&index)
            ->AddArg(&f_one)
            ->SetResult(&index);
      }
    } else {
      FScopedSlot length(this);
      Add(new FSizeof())->AddArg(load_arg->result)->SetResult(&length);

      // By length of vararg
      Add(new FBinOp(BinOp::kAdd))
          ->AddArg(&index)
          ->AddArg(&length)
          ->SetResult(&index);
    }
  }
}


FInstruction* Fullgen::VisitFunction(AstNode* stmt) {
  FunctionLiteral* fn = FunctionLiteral::Cast(stmt);

  if (fn->label() == NULL) {
    fn->label(new Label());
  }

  if (current_function()->root_ast() == stmt) {
    current_function()->body = new FLabel(fn->label());
    Add(current_function()->body);
    current_function()->entry = new FEntry(stmt->context_slots());
    Add(current_function()->entry);

    // Load all passed arguments
    LoadArguments(fn);

    // Generate body
    VisitChildren(stmt);

    // In case if there're no return statement in function
    {
      FScopedSlot result(this);

      FInstruction* arg = Add(new FNil());
      arg->SetResult(&result);
      Add(new FReturn())->AddArg(&result);
    }

    return NULL;
  } else {
    FFunction* f = new FFunction(stmt, fn->args()->length());
    Add(f);

    f->body = new FLabel(fn->label());

    return f;
  }
}


FInstruction* Fullgen::VisitReturn(AstNode* node) {
  FScopedSlot result(this);
  Visit(node->lhs())->SetResult(&result);

  return Add(new FReturn())->AddArg(&result);
}


FInstruction* Fullgen::VisitLiteral(AstNode* node) {
  return Add(new FLiteral(node->type(), root_->Put(node)));
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
  Visit(stmt->rhs())->SetResult(&rhs);

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

  return Add(new FChi())->AddArg(&rhs);
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
  Add(new FDeleteProperty())->AddArg(&recv)->AddArg(&prop);
  return Add(new FNil());
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
  FInstruction* rhs = Visit(node->rhs());
  if (rhs != NULL) rhs->SetResult(&cond);

  Add(new FGoto(join));
  Add(f);
  AstList::Item* else_branch = node->children()->head()->next()->next();
  if (else_branch != NULL) {
    FInstruction* e =Visit(else_branch->value());
    if (e != NULL) e->SetResult(&cond);
  }

  Add(join);

  return NULL;
}


FInstruction* Fullgen::VisitWhile(AstNode* node) {
  FScopedSlot cond(this);

  FLabel* prev_start = loop_start_;
  FLabel* prev_end = loop_end_;

  loop_start_ = new FLabel();
  FLabel* body = new FLabel();
  loop_end_ = new FLabel();

  Add(loop_start_);
  Visit(node->lhs())->SetResult(&cond);
  Add(new FIf(body, loop_end_))->AddArg(&cond);

  Add(body);
  FInstruction* rhs = Visit(node->rhs());
  if (rhs != NULL) rhs->SetResult(&cond);

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
  FunctionLiteral* fn = FunctionLiteral::Cast(stmt);

  // handle __$gc() and __$trace() calls
  if (fn->variable()->is(AstNode::kValue)) {
    AstNode* name = AstValue::Cast(fn->variable())->name();
    if (name->length() == 5 && strncmp(name->value(), "__$gc", 5) == 0) {
      Add(new FCollectGarbage());
      return Add(new FNil());
    } else if (name->length() == 8 &&
               strncmp(name->value(), "__$trace", 8) == 0) {
      return Add(new FGetStackTrace());
    }
  }

  // Generate all arg's values and populate list of stores
  FInstruction* vararg = NULL;
  FOperandList arg_slots;
  FInstructionList stores_;
  AstList::Item* item = fn->args()->head();
  for (; item != NULL; item = item->next()) {
    AstNode* arg = item->value();
    FInstruction* current;
    FInstruction* rhs;

    if (arg->is(AstNode::kSelf)) {
      // Process self argument later
      continue;
    } else if (arg->is(AstNode::kVarArg)) {
      current = new FStoreVarArg();
      rhs = Visit(arg->lhs());
      vararg = rhs;
    } else {
      current = new FStoreArg();
      rhs = Visit(arg);
    }

    FOperand* slot = GetSlot();
    rhs->SetResult(slot);
    current->AddArg(slot);
    arg_slots.Push(slot);

    stores_.Unshift(current);
  }

  // Determine argc and alignment
  int argc = fn->args()->length();
  if (vararg != NULL) argc--;

  FScopedSlot argc_slot(this);
  GetNumber(argc)->SetResult(&argc_slot);

  // If call has vararg - increase argc by ...
  FScopedSlot length(this);
  if (vararg != NULL) {
    Add(new FSizeof())
        ->AddArg(vararg->result)
        ->SetResult(&length);

    // ... by the length of vararg
    Add(new FBinOp(BinOp::kAdd))
        ->AddArg(&argc_slot)
        ->AddArg(&length)
        ->SetResult(&argc_slot);
  }

  // Process self argument
  FInstruction* receiver = NULL;
  if (fn->args()->length() > 0 &&
      fn->args()->head()->value()->is(AstNode::kSelf)) {
    FOperand* slot = GetSlot();
    arg_slots.Push(slot);
    receiver = Visit(fn->variable()->lhs())->SetResult(slot);

    FInstruction* store = new FStoreArg();
    store->AddArg(slot);
    stores_.Push(store);
  }

  FInstruction* var;
  if (fn->args()->length() > 0 &&
      fn->args()->head()->value()->is(AstNode::kSelf)) {
    assert(fn->variable()->is(AstNode::kMember));

    FScopedSlot property(this);
    Visit(fn->variable()->rhs())->SetResult(&property);

    var = Add(new FLoadProperty())
        ->AddArg(receiver->result)
        ->AddArg(&property);
  } else {
    var = Visit(fn->variable());
  }

  // Add stack alignment instruction
  Add(new FAlignStack())->AddArg(&argc_slot);

  // Add indexes to stores
  FInstruction* index = GetNumber(0);
  bool seen_varg = false;
  FInstructionList::Item* htail = stores_.tail();
  for (int i = 0; htail != NULL; htail = htail->prev(), i++) {
    FInstruction* store = htail->value();

    // Allocate slot
    FOperand* slot = GetSlot();
    arg_slots.Push(slot);
    index->SetResult(slot);

    if (store->type() == FInstruction::kStoreVarArg) {
      AstNode* one = new AstNode(AstNode::kNumber, stmt);
      FScopedSlot f_one(this);

      one->value("1");
      one->length(1);

      Add(new FBinOp(BinOp::kAdd))
          ->AddArg(slot)
          ->AddArg(&length)
          ->SetResult(slot);
      Visit(one)->SetResult(&f_one);
      Add(new FBinOp(BinOp::kSub))
          ->AddArg(slot)
          ->AddArg(&f_one)
          ->SetResult(slot);
      seen_varg = true;
    }

    // Add index as argument
    store->AddArg(slot);

    // No need to recalculate index after last argument
    if (htail->prev() == NULL) continue;

    if (seen_varg) {
      AstNode* one = new AstNode(AstNode::kNumber, stmt);
      FScopedSlot f_one(this);

      one->value("1");
      one->length(1);

      Visit(one)->SetResult(&f_one);
      index = Add(new FBinOp(BinOp::kAdd))
          ->AddArg(slot)
          ->AddArg(&f_one);
    } else {
      index = GetNumber(i + 1);
    }
  }

  if (index->result == NULL) {
    FOperand* slot = GetSlot();
    arg_slots.Push(slot);
    index->SetResult(slot);
  }

  // Now add stores to hir
  FInstructionList::Item* hhead = stores_.head();
  for (; hhead != NULL; hhead = hhead->next()) {
    Add(hhead->value());
  }

  FScopedSlot var_slot(this);
  var->SetResult(&var_slot);

  // Release slots used for arguments
  while (arg_slots.length() > 0) ReleaseSlot(arg_slots.Shift());

  return Add(new FCall())->AddArg(&var_slot)->AddArg(&argc_slot);
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

  if (!BinOp::is_bool_logic(op->subtype())) {
    FScopedSlot rhs(this);
    Visit(node->rhs())->SetResult(&rhs);

    return Add(new FBinOp(op->subtype()))->AddArg(&lhs)->AddArg(&rhs);
  } else {
    FScopedSlot result(this);
    FLabel* t = new FLabel();
    FLabel* f = new FLabel();
    FLabel* join = new FLabel();
    Add(new FIf(t, f))->AddArg(&lhs);

    Add(t);
    if (op->subtype() == BinOp::kLAnd) {
      Visit(op->rhs())->SetResult(&result);
    } else {
      Add(new FChi())->AddArg(&lhs)->SetResult(&result);
    }

    Add(new FGoto(join));
    Add(f);
    if (op->subtype() == BinOp::kLAnd) {
      Add(new FChi())->AddArg(&lhs)->SetResult(&result);
    } else {
      Visit(op->rhs())->SetResult(&result);
    }
    Add(join);

    return Add(new FChi())->AddArg(&result);
  }
}


void Fullgen::Print(PrintBuffer* p) {
  FInstructionList::Item* ihead = instructions_.head();
  for (; ihead != NULL; ihead = ihead->next()) {
    ihead->value()->Print(p);
  }
}


void FAlignCode::Generate(Masm* masm) {
  masm->AlignCode();
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
    default: UNEXPECTED
  }
}

}  // namespace internal
}  // namespace candor
