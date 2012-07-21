#include "hir.h"
#include "hir-instructions.h"
#include "lir-allocator.h"
#include "lir-allocator-inl.h"
#include "lir-instructions.h" // LIRInstruction
#include "visitor.h" // Visitor
#include "ast.h" // AstNode
#include "macroassembler.h" // Masm, RelocationInfo
#include "utils.h" // List

#include <stdlib.h> // NULL
#include <stdint.h> // int64_t
#include <sys/types.h> // off_t
#include <assert.h> // assert

#define __STDC_FORMAT_MACROS
#include <inttypes.h> // printf formats for big integers

namespace candor {
namespace internal {

HIRBasicBlock::HIRBasicBlock(HIR* hir) : hir_(hir),
                                         type_(kNormal),
                                         enumerated_(0),
                                         prev_(NULL),
                                         next_(NULL),
                                         dominator_(NULL),
                                         predecessor_count_(0),
                                         successor_count_(0),
                                         masm_(NULL),
                                         loop_start_(NULL),
                                         finished_(false),
                                         id_(-1) {
  predecessors_[0] = NULL;
  predecessors_[1] = NULL;
  successors_[0] = NULL;
  successors_[1] = NULL;
}


void HIRBasicBlock::AddValue(HIRValue* value) {
  // Do not add parasite values: immediate or root values
  if (value->slot()->is_immediate() ||
      (value->slot()->is_context() && value->slot()->depth() < 0)) {
    return;
  }

  // Do not insert values after enumeration
  if (value->block()->hir()->enumerated()) return;

  // If value is already in values list - remove previous
  HIRValueList::Item* item = values()->head();
  for (; item != NULL; item = item->next()) {
    if (item->value()->slot() == value->slot()) {
      values()->Remove(item);
    }
  }
  values()->Push(value);
}


void HIRBasicBlock::AssignDominator(HIRBasicBlock* block) {
  if (dominator() == NULL) {
    dominator(block);
    return;
  }

  HIRBasicBlock* d = dominator();
  while (!d->Dominates(block)) {
    assert(d->predecessor_count() > 0);
    d = d->predecessors()[0];
  }

  dominator(d);
}


void HIRBasicBlock::AddPredecessor(HIRBasicBlock* block) {
  assert(predecessor_count() < 2);
  predecessors()[predecessor_count_++] = block;

  HIRValueList::Item* item;

  // Assign dominator
  AssignDominator(block);

  // Mark values propagated from first predecessor
  if (predecessor_count() > 1) {
    for (item = values()->head(); item != NULL; item = item->next()) {
      if (item->value() == NULL) continue;

      item->value()->current_block(this);
      item->value()->slot()->hir(item->value());
    }
  }

  // Lift values from new predecessor
  for (item = block->values()->head(); item != NULL; item = item->next()) {
    HIRValue* value = item->value();
    HIRPhi* phi;
    assert(value->slot()->hir() != NULL);

    // Value is already in values() list
    if (value->slot()->hir()->current_block() == this) {
      if (is_loop_start()) {
        // Only phis are in the values list for loop start
        phi = HIRPhi::Cast(value->slot()->hir());
      } else {
        phi = new HIRPhi(this, value->slot()->hir());
        AddValue(phi);
        value->slot()->hir(phi);
      }

      phi->AddInput(value);
    } else {
      if (is_loop_start()) {
        // Insert phi for every local variable in loop start
        phi = new HIRPhi(this, value);
        AddValue(phi);
        value->slot()->hir(phi);
      } else {
        // Just put value into the list
        AddValue(value);
        value->current_block(this);
        value->slot()->hir(value);
      }
    }
  }
}


void HIRBasicBlock::AddSuccessor(HIRBasicBlock* block) {
  assert(successor_count() < 2);
  successors()[successor_count_++] = block;
  block->AddPredecessor(this);
}


void HIRBasicBlock::Goto(HIRBasicBlock* block) {
  if (finished()) return;

  // Add move in case if we're going to resolve phis here later
  HIRPhiMove* move = new HIRPhiMove();
  instructions()->Push(move);
  move->Init(this);

  // Add goto instruction and finalize block
  HIRGoto* instr = new HIRGoto();
  instructions()->Push(instr);
  instr->Init(this);
  finished(true);

  // Connect graph nodes
  AddSuccessor(block);
}



HIRInstruction* HIRBasicBlock::FindInstruction(int id) {
  HIRInstruction* instr = last_instruction();

  for (; instr != NULL; instr = instr->prev()) {
    if (instr->lir() != NULL && instr->lir()->id() == id) {
      return instr;
    }
  }

  return NULL;
}


bool HIRBasicBlock::Dominates(HIRBasicBlock* block) {
  HIRBasicBlock* d = block;
  while (d != NULL && d != this) {
    d = d->dominator();
  }

  return d != NULL;
}


void HIRBasicBlock::PrunePhis() {
  HIRPhiList::Item* item;
  for (item = phis()->head(); item != NULL; item = item->next()) {
    HIRPhi* phi = item->value();
    if (phi->inputs()->length() < 2) {
      phi->Replace(phi->inputs()->head()->value());
      phis()->Remove(item);
    }
  }
}


bool HIRBasicBlock::IsPrintable() {
  off_t value = MarkPrinted();

  if (predecessor_count() == 2 &&
      (Dominates(predecessors()[0]) || Dominates(predecessors()[1]))) {
    return value == 1;
  }
  if (value == predecessor_count()) return true;

  return false;
}


off_t HIRBasicBlock::MarkPrinted() {
  off_t value = reinterpret_cast<off_t>(
      hir()->print_map()->Get(NumberKey::New(id()))) + 1;
  hir()->print_map()->Set(NumberKey::New(id()),
                          reinterpret_cast<int*>(value));
  return value;
}


void HIRBasicBlock::Print(PrintBuffer* p) {
  p->Print("[Block#%d", id());

  if (live_gen()->length() > 0) {
    p->Print(" gen:[");
    HIRValueList::Item* item;
    for (item = live_gen()->head(); item != NULL; item = item->next()) {
      p->Print(item->next() != NULL ? "%d," : "%d", item->value()->id());
    }
    p->Print("]");
  }

  if (live_kill()->length() > 0) {
    p->Print(" kill:[");
    HIRValueList::Item* item;
    for (item = live_kill()->head(); item != NULL; item = item->next()) {
      p->Print(item->next() != NULL ? "%d," : "%d", item->value()->id());
    }
    p->Print("]");
  }

  if (live_in()->length() > 0) {
    p->Print(" in:[");
    HIRValueList::Item* item;
    for (item = live_in()->head(); item != NULL; item = item->next()) {
      p->Print(item->next() != NULL ? "%d," : "%d", item->value()->id());
    }
    p->Print("]");
  }

  if (live_out()->length() > 0) {
    p->Print(" out:[");
    HIRValueList::Item* item;
    for (item = live_out()->head(); item != NULL; item = item->next()) {
      p->Print(item->next() != NULL ? "%d," : "%d", item->value()->id());
    }
    p->Print("]");
  }

  // Print phis
  {
    HIRPhiList::Item* item = phis()->head();
    if (item != NULL) p->Print(" ");
    while (item != NULL) {
      item->value()->Print(p);
      item = item->next();
      if (item != NULL) p->Print(" ");
    }
  }

  // Print instructions
  {
    HIRInstructionList::Item* item = instructions()->head();
    if (item != NULL) p->Print("\n");
    for (; item != NULL; item = item->next()) {
      item->value()->Print(p);
    }
  }

  // Print predecessors' ids
  if (predecessor_count() == 2) {
    p->Print("[%d,%d]", predecessors()[0]->id(), predecessors()[1]->id());
  } else if (predecessor_count() == 1) {
    p->Print("[%d]", predecessors()[0]->id());
  } else {
    p->Print("[]");
  }

  p->Print(">*>");

  // Print successors' ids
  if (successor_count() == 2) {
    p->Print("[%d,%d]", successors()[0]->id(), successors()[1]->id());
  } else if (successor_count() == 1) {
    p->Print("[%d]", successors()[0]->id());
  } else {
    p->Print("[]");
  }

  p->Print("]\n\n");

  // Print successors
  if (successor_count() == 2) {
    if (successors()[0]->IsPrintable()) successors()[0]->Print(p);
    if (successors()[1]->IsPrintable()) successors()[1]->Print(p);
  } else if (successor_count() == 1) {
    if (successors()[0]->IsPrintable()) successors()[0]->Print(p);
  }
}


HIRPhi::HIRPhi(HIRBasicBlock* block, HIRValue* value)
    : HIRValue(kPhi, block, value->slot()) {
  type_ = kPhi;
  AddInput(value);

  // Add Phi to block's phis
  block->phis()->Push(this);

  // Extend global lists
  block->hir()->phis()->Push(this);
  block->hir()->values()->Push(this);
}


void HIRPhi::AddInput(HIRValue* input) {
  assert(inputs()->length() < 2);

  if (input == this ||
      (inputs()->length() > 0 && inputs()->head()->value() == input)) {
    return;
  }

  inputs()->Push(input);
  input->phi_uses()->Push(this);
}


void HIRPhi::ReplaceVarUse(HIRValue* source, HIRValue* target) {
  HIRValueList::Item* item;
  for (item = inputs()->head(); item != NULL; item = item->next()) {
    if (item->value() == source) {
      if (target == this) {
        inputs()->Remove(item);
        block()->PrunePhis();
      } else {
        item->value(target);
      }
    }
  }
}


void HIRPhi::Print(PrintBuffer* p) {
  p->Print("@[");
  HIRValueList::Item* item = inputs()->head();
  while (item != NULL) {
    p->Print("%d", item->value()->id());
    item = item->next();
    if (item != NULL) p->Print(",");
  }
  p->Print("]:%d", id());
}


HIRValue::HIRValue(HIRBasicBlock* block) : type_(kNormal),
                                           block_(block),
                                           current_block_(block),
                                           lir_(NULL) {
  slot_ = new ScopeSlot(ScopeSlot::kStack);
  Init();
}


HIRValue::HIRValue(HIRBasicBlock* block, ScopeSlot* slot)
    : type_(kNormal),
      block_(block),
      current_block_(block),
      lir_(NULL),
      slot_(slot) {
  Init();
}


HIRValue::HIRValue(ValueType type, HIRBasicBlock* block, ScopeSlot* slot)
    : type_(type),
      block_(block),
      current_block_(block),
      lir_(NULL),
      slot_(slot) {
  Init();
}


void HIRValue::Init() {
  block()->AddValue(this);
  id_ = block()->hir()->get_variable_index();

  lir_ = new LIRValue(this);
}


void HIRValue::Print(PrintBuffer* p) {
  p->Print("*[%d ", id());
  slot()->Print(p);
  p->Print("]");

  if (lir()->interval()->start() != -1) {
    p->Print(" [%d,%d)", lir()->interval()->start(), lir()->interval()->end());
  }
}


void HIRValue::Print(PrintBuffer* p, HIRInstruction* instr) {
  p->Print("*[%d ", id());
  slot()->Print(p);
  p->Print("]");

  LIRInterval* interval = lir()->interval()->ChildAt(instr->id());
  if (interval != NULL && interval->start() != -1) {
    p->Print(" [%d,%d)", interval->start(), interval->end());
    if (interval->operand() != NULL) {
      p->Print(":");
      interval->operand()->Print(p);
    }
  }
}


void HIRValue::Replace(HIRValue* target) {
  {
    HIRInstructionList::Item* item;
    for (item = uses()->head(); item != NULL; item = item->next()) {
      item->value()->ReplaceVarUse(this, target);
    }
  }

  {
    HIRPhiList::Item* item;
    for (item = phi_uses()->head(); item != NULL; item = item->next()) {
      item->value()->ReplaceVarUse(this, target);
    }
  }
}


bool HIRValue::IsIn(HIRValueList* list) {
  HIRValueList::Item* item = list->head();
  for (; item != NULL; item = item->next()) {
    if (item->value() == this) return true;
  }

  return false;
}


HIRBreakContinueInfo::HIRBreakContinueInfo(HIR* hir,  AstNode* node)
    : Visitor(kPreorder),
      hir_(hir),
      previous_(hir->break_continue_info()),
      continue_count_(0) {

  continue_blocks()->Push(hir->current_block());
  AddBlock(kContinueBlocks);
  continue_blocks()->Shift();

  AddBlock(kBreakBlocks);

  hir->break_continue_info(this);

  VisitChildren(node);
}


HIRBreakContinueInfo::~HIRBreakContinueInfo() {
  hir()->break_continue_info(previous_);
}


HIRBasicBlock* HIRBreakContinueInfo::AddBlock(ListKind kind) {
  HIRBasicBlock* block;
  HIRBasicBlockList* list;

  if (kind == kContinueBlocks) {
    block = hir()->CreateLoopStart();
    list = continue_blocks();
    hir()->set_current_block(block);
  } else {
    block = hir()->CreateBlock();
    list = break_blocks();
  }

  if (list->tail() != NULL) list->tail()->value()->Goto(block);
  list->Push(block);

  return block;
}


HIR::HIR(Heap* heap, AstNode* node) : Visitor(kPreorder),
                                      root_(heap),
                                      first_block_(NULL),
                                      break_continue_info_(NULL),
                                      first_instruction_(NULL),
                                      last_instruction_(NULL),
                                      variable_index_(0),
                                      instruction_index_(0),
                                      enumerated_(false),
                                      print_map_(NULL) {
  work_list_.Push(new HIRFunction(node, CreateBlock()));

  HIRFunction* fn;
  while ((fn = work_list_.Shift()) != NULL) {
    root_block_ = fn->block();
    roots()->Push(fn->block());
    set_current_block(fn->block());
    Visit(fn->node());
  }

  Enumerate();
}


HIRValue* HIR::FindPredecessorValue(ScopeSlot* slot) {
  assert(current_block() != NULL);

  HIRBasicBlock* block = current_block();
  while (block != NULL) {
    HIRValueList::Item* item = block->values()->head();
    for (; item != NULL; item = item->next()) {
      if (item->value()->slot() == slot) {
        return item->value();
      }
    }

    block = block->dominator();
  }

  return NULL;
}


HIRValue* HIR::CreateValue(HIRBasicBlock* block, ScopeSlot* slot) {
  HIRValue* value = new HIRValue(block, slot);
  HIRValue* previous = FindPredecessorValue(slot);

  slot->hir(value);

  // Push value to the values list
  values()->Push(value);

  return value;
}


HIRValue* HIR::CreateValue(HIRBasicBlock* block) {
  return CreateValue(block, new ScopeSlot(ScopeSlot::kStack));
}


HIRValue* HIR::CreateValue(ScopeSlot* slot) {
  return CreateValue(current_block(), slot);
}


HIRValue* HIR::GetValue(ScopeSlot* slot) {
  assert(current_block() != NULL);

  // Slot was used - find one in our branch
  HIRValue* previous = FindPredecessorValue(slot);

  // Lazily create new variable
  if (previous == NULL) {
    // Slot wasn't used in HIR yet
    // Insert new one HIRValue in the current block
    CreateValue(slot);
  } else {
    slot->hir(previous);
    slot->hir()->current_block(current_block());
  }

  return slot->hir();
}


HIRValue* HIR::GetNil() {
  HIRValue* Nil = CreateValue(root()->Put(new AstNode(AstNode::kNil)));
  AddInstruction(new HIRLoadRoot(Nil));
  return Nil;
}


HIRBasicBlock* HIR::CreateBlock() {
  return new HIRBasicBlock(this);
}


HIRLoopStart* HIR::CreateLoopStart() {
  return new HIRLoopStart(this);
}


HIRBasicBlock* HIR::CreateJoin(HIRBasicBlock* left, HIRBasicBlock* right) {
  HIRBasicBlock* join = CreateBlock();

  left->Goto(join);
  right->Goto(join);

  return join;
}


void HIR::AddInstruction(HIRInstruction* instr) {
  assert(current_block() != NULL);
  instr->Init(current_block());
  instr->ast(current_node());

  if (current_block()->finished()) return;

  current_block()->instructions()->Push(instr);
}


void HIR::Finish(HIRInstruction* instr) {
  assert(current_block() != NULL);
  AddInstruction(instr);
  current_block()->finished(true);
}


void HIR::Enumerate() {
  HIRBasicBlockList::Item* root = roots()->head();
  HIRBasicBlockList work_list;

  // Add roots to worklist
  for (; root != NULL; root = root->next()) {
    root->value()->enumerate();
    work_list.Push(root->value());
  }

  // Setup last/first instruction
  first_instruction(work_list.head()->value()->first_instruction());
  last_instruction(NULL);

  // Process worklist
  HIRBasicBlock* current;
  HIRBasicBlock* previous = NULL;
  int block_id = 0;
  while ((current = work_list.Shift()) != NULL) {
    // Create list of blocks in enumeration order
    if (previous != NULL) {
      previous->next(current);
    } else {
      first_block(current);
    }
    current->prev(previous);
    previous = current;

    // Insert nop instruction in empty blocks
    if (current->instructions()->length() == 0) {
      set_current_block(current);
      AddInstruction(new HIRNop());
    }

    // Prune phis with less than 2 inputs
    current->PrunePhis();

    if (current->id() == -1) {
      current->id(block_id++);
    }

    // Go through all block's instructions, link them together and assign id
    HIRInstructionList::Item* instr = current->instructions()->head();
    for (; instr != NULL; instr = instr->next()) {
      // Insert instruction into linked list
      if (last_instruction() != NULL) last_instruction()->next(instr->value());

      instr->value()->prev(last_instruction());
      instr->value()->id(get_instruction_index());
      last_instruction(instr->value());
    }

    for (int i = 0; i < current->successor_count(); i++) {
      if (current->successors()[i]->id() == -1) {
        current->successors()[i]->id(block_id++);
      }
    }

    // Add block's successors to the work list
    for (int i = current->successor_count() - 1; i >= 0; i--) {
      // Skip processed blocks and join blocks that was visited only once
      current->successors()[i]->enumerate();
      if (current->successors()[i]->is_enumerated()) {
        work_list.Unshift(current->successors()[i]);
      }
    }
  }

  enumerated(true);
}


HIRValue* HIR::GetValue(AstNode* node) {
  Visit(node);

  if (current_block() == NULL ||
      current_block()->instructions()->length() == 0) {
    return GetNil();
  } else {
    return GetLastResult();
  }
}


HIRValue* HIR::GetLastResult() {
  HIRInstruction* instr = current_block()->instructions()->tail()->value();

  HIRValue* res = instr->is(HIRInstruction::kNop) ?
      instr->values()->head()->value()
      :
      instr->GetResult();

  if (res == NULL) {
    return GetNil();
  } else {
    return res;
  }
}


void HIR::Print(char* buffer, uint32_t size) {
  PrintMap map;

  PrintBuffer p(buffer, size);
  print_map(&map);

  HIRBasicBlockList::Item* item = roots()->head();
  for (; item != NULL; item = item->next()) {
    item->value()->Print(&p);
  }

  print_map(NULL);
  p.Finalize();
}


AstNode* HIR::VisitFunction(AstNode* stmt) {
  FunctionLiteral* fn = FunctionLiteral::Cast(stmt);

  if (current_block() == root_block() &&
      current_block()->instructions()->length() == 0) {
    HIREntry* entry = new HIREntry(fn->context_slots());

    AddInstruction(entry);

    // Add agruments
    AstList::Item* arg = fn->args()->head();
    for (; arg != NULL; arg = arg->next()) {
      AstValue* value = AstValue::Cast(arg->value());
      HIRValue* hir_value;
      if (value->slot()->is_context()) {
        // Create temporary on-stack variable
        hir_value = CreateValue(current_block());

        // And move it into context
        AddInstruction(new HIRStoreContext(GetValue(value->slot()),
                                           hir_value));
      } else {
        hir_value = GetValue(value->slot());
      }
      entry->AddArg(hir_value);
    }

    VisitChildren(stmt);

    AddInstruction(new HIRReturn(GetNil()));
  } else {
    HIRBasicBlock* block = CreateBlock();
    AddInstruction(new HIRAllocateFunction(block, fn->args()->length()));

    work_list_.Push(new HIRFunction(stmt, block));
  }

  return stmt;
}


AstNode* HIR::VisitAssign(AstNode* stmt) {
  HIRValue* rhs;
  if (stmt->lhs()->is(AstNode::kValue)) {
    rhs = GetValue(stmt->rhs());

    AstValue* value = AstValue::Cast(stmt->lhs());
    HIRValue* lhs = CreateValue(value->slot());

    if (value->slot()->is_stack()) {
      AddInstruction(new HIRStoreLocal(lhs, rhs));
    } else {
      AddInstruction(new HIRStoreContext(lhs, rhs));
    }
  } else if (stmt->lhs()->is(AstNode::kMember)) {
    rhs = GetValue(stmt->rhs());
    HIRValue* property = GetValue(stmt->lhs()->rhs());
    HIRValue* receiver = GetValue(stmt->lhs()->lhs());

    AddInstruction(new HIRStoreProperty(receiver, property, rhs));

    // Propagate result
    AddInstruction(new HIRNop(rhs));
  } else {
    // TODO: Set error! Incorrect lhs
    abort();
  }

  return stmt;
}


AstNode* HIR::VisitValue(AstNode* node) {
  AstValue* value = AstValue::Cast(node);
  if (value->slot()->is_stack()) {
    AddInstruction(new HIRNop(GetValue(value->slot())));
  } else {
    AddInstruction(new HIRLoadContext(GetValue(value->slot())));
  }
  return node;
}


void HIR::VisitRootValue(AstNode* node) {
  AddInstruction(new HIRLoadRoot(CreateValue(root()->Put(node))));
}


AstNode* HIR::VisitIf(AstNode* node) {
  HIRBasicBlock* on_true = CreateBlock();
  HIRBasicBlock* on_false = CreateBlock();
  HIRBasicBlock* join = NULL;
  HIRBranchBool* branch = new HIRBranchBool(GetValue(node->lhs()),
                                            on_true,
                                            on_false);
  Finish(branch);

  set_current_block(on_true);
  Visit(node->rhs());
  on_true = current_block();

  AstList::Item* else_body = node->children()->head()->next()->next();
  set_current_block(on_false);

  if (else_body != NULL) {
    // Visit else body and create additional `join` block
    Visit(else_body->value());
  }

  on_false = current_block();

  set_current_block(CreateJoin(on_true, on_false));

  return node;
}


AstNode* HIR::VisitWhile(AstNode* node) {
  HIRBreakContinueInfo b(this, node);
  HIRBasicBlock* body = CreateBlock();
  HIRBasicBlock* cond = CreateBlock();

  //   entry
  //     |
  // [continue blocks]
  //  /  |
  // |  cond
  // |/  |
  // ^   |
  //  \  |
  //    body
  //     |  \
  //     |   |
  //     |  /
  //   [break blocks]
  //     |
  //   [end]

  // Create new block and insert condition expression into it
  current_block()->Goto(cond);
  set_current_block(cond);
  HIRBranchBool* branch = new HIRBranchBool(GetValue(node->lhs()),
                                            body,
                                            b.first_break_block());
  Finish(branch);

  // Generate loop's body
  set_current_block(body);
  Visit(node->rhs());

  // And loop it back to condition
  current_block()->Goto(b.first_continue_block());
  b.first_continue_block()->loop(current_block());
  b.first_continue_block()->end(b.first_break_block());

  // Execution will continue in the `end` block
  set_current_block(b.last_break_block());

  return node;
}


AstNode* HIR::VisitMember(AstNode* node) {
  HIRValue* property = GetValue(node->rhs());
  HIRValue* receiver = GetValue(node->lhs());

  AddInstruction(new HIRLoadProperty(receiver, property));

  return node;
}


AstNode* HIR::VisitCall(AstNode* stmt) {
  FunctionLiteral* fn = FunctionLiteral::Cast(stmt);

  // handle __$gc() and __$trace() calls
  if (fn->variable()->is(AstNode::kValue)) {
    AstNode* name = AstValue::Cast(fn->variable())->name();
    if (name->length() == 5 && strncmp(name->value(), "__$gc", 5) == 0) {
      AddInstruction(new HIRCollectGarbage());
      return stmt;
    } else if (name->length() == 8 &&
               strncmp(name->value(), "__$trace", 8) == 0) {
      AddInstruction(new HIRGetStackTrace());
      return stmt;
    }
  }

  HIRValue* var;
  HIRValue* receiver = NULL;
  if (fn->args()->length() > 0 &&
      fn->args()->head()->value()->is(AstNode::kSelf)) {
    assert(fn->variable()->is(AstNode::kMember));

    receiver = GetValue(fn->variable()->lhs());
    HIRValue* property = GetValue(fn->variable()->rhs());

    AddInstruction(new HIRLoadProperty(receiver, property));
    var = GetLastResult();
  } else {
    var = GetValue(fn->variable());
  }
  HIRCall* call = new HIRCall(var);

  AstList::Item* item = fn->args()->head();
  for (; item != NULL; item = item->next()) {
    if (item->value()->is(AstNode::kSelf)) {
      assert(receiver != NULL);
      call->AddArg(receiver);
    } else {
      call->AddArg(GetValue(item->value()));
    }
  }

  AddInstruction(call);

  return stmt;
}


void HIR::VisitGenericObject(AstNode* node) {
  int size = node->children()->length();
  HIRAllocateObject::ObjectKind kind;
  switch (node->type()) {
   case AstNode::kObjectLiteral:
    kind = HIRAllocateObject::kObject;
    break;
   case AstNode::kArrayLiteral:
    kind = HIRAllocateObject::kArray;
    break;
   default: UNEXPECTED break;
  }

  // Create object
  HIRAllocateObject* instr = new HIRAllocateObject(kind, size);
  AddInstruction(instr);

  // Get result
  HIRValue* result = instr->GetResult();

  // Insert properties
  switch (node->type()) {
   case AstNode::kObjectLiteral:
    {
      ObjectLiteral* obj = ObjectLiteral::Cast(node);

      assert(obj->keys()->length() == obj->values()->length());
      AstList::Item* key = obj->keys()->head();
      AstList::Item* value = obj->values()->head();
      while (key != NULL) {
        AddInstruction(new HIRStoreProperty(
              result,
              GetValue(key->value()),
              GetValue(value->value())));

        key = key->next();
        value = value->next();
      }
    }
    break;
   case AstNode::kArrayLiteral:
    {
      AstList::Item* item = node->children()->head();
      uint64_t index = 0;
      while (item != NULL) {
        char keystr[32];
        AstNode* key = new AstNode(AstNode::kNumber, node);
        key->value(keystr);
        key->length(snprintf(keystr, sizeof(keystr), "%" PRIu64, index));

        AddInstruction(new HIRStoreProperty(
              result,
              CreateValue(root()->Put(key)),
              GetValue(item->value())));

        item = item->next();
        index++;
      }
    }
    break;
   default: UNEXPECTED break;
  }

  // And return object
  AddInstruction(new HIRNop(result));
  // Rewrite previous declarations of variable
  current_block()->AddValue(result);
}


AstNode* HIR::VisitReturn(AstNode* node) {
  HIRValue* result = NULL;
  if (node->lhs() != NULL) result = GetValue(node->lhs());

  if (result == NULL) {
    result = GetNil();
  }
  Finish(new HIRReturn(result));

  return node;
}


AstNode* HIR::VisitClone(AstNode* node) {
  AddInstruction(new HIRCloneObject(GetValue(node->lhs())));

  return node;
}


AstNode* HIR::VisitDelete(AstNode* node) {
  // TODO: Set error
  assert(node->lhs()->is(AstNode::kMember));

  HIRValue* property = GetValue(node->lhs()->rhs());
  HIRValue* receiver = GetValue(node->lhs()->lhs());

  AddInstruction(new HIRDeleteProperty(receiver, property));

  return node;
}


AstNode* HIR::VisitBreak(AstNode* node) {
  // TODO: Set error
  assert(break_continue_info() != NULL);

  HIRBasicBlock* b = break_continue_info()->AddBlock(
      HIRBreakContinueInfo::kBreakBlocks);
  current_block()->Goto(b);

  return node;
}


AstNode* HIR::VisitContinue(AstNode* node) {
  // TODO: Set error
  assert(break_continue_info() != NULL);

  HIRBasicBlock* block = break_continue_info()->continue_blocks()->Pop();
  current_block()->Goto(block);

  HIRLoopStart::Cast(block)->loop(current_block());
  HIRLoopStart::Cast(block)->end(break_continue_info()->first_break_block());

  return node;
}


AstNode* HIR::VisitTypeof(AstNode* node) {
  AddInstruction(new HIRTypeof(GetValue(node->lhs())));
  return node;
}


AstNode* HIR::VisitSizeof(AstNode* node) {
  AddInstruction(new HIRSizeof(GetValue(node->lhs())));
  return node;
}


AstNode* HIR::VisitKeysof(AstNode* node) {
  AddInstruction(new HIRKeysof(GetValue(node->lhs())));
  return node;
}


AstNode* HIR::VisitUnOp(AstNode* node) {
  UnOp* op = UnOp::Cast(node);

  // Changing ops should be translated into another form
  if (op->is_changing()) {
    AstNode* one = new AstNode(AstNode::kNumber, node);
    one->value("1");
    one->length(1);

    if (op->is_postfix()) {
      // a++ => t = a, a = t + 1, t

      AstValue* tmp = new AstValue(AstValue::Cast(op->lhs())->slot(),
                                   AstValue::Cast(op->lhs())->name());
      AstNode* init = new AstNode(AstNode::kAssign);
      init->children()->Push(tmp);
      init->children()->Push(op->lhs());

      HIRValue* result = GetValue(init);

      AstNode* change = new AstNode(AstNode::kAssign);
      change->children()->Push(op->lhs());
      switch (op->subtype()) {
       case UnOp::kPostInc:
        change->children()->Push(new BinOp(BinOp::kAdd, tmp, one));
        break;
       case UnOp::kPostDec:
        change->children()->Push(new BinOp(BinOp::kSub, tmp, one));
        break;
       default:
        UNEXPECTED
        break;
      }
      Visit(change);

      // return result
      AddInstruction(new HIRNop(result));
    } else {
      // ++a => a = a + 1
      AstNode* rhs = NULL;
      switch (op->subtype()) {
       case UnOp::kPreInc:
        rhs = new BinOp(BinOp::kAdd, op->lhs(), one);
        break;
       case UnOp::kPreDec:
        rhs = new BinOp(BinOp::kSub, op->lhs(), one);
        break;
       default:
        UNEXPECTED
        break;
      }

      AstNode* assign = new AstNode(AstNode::kAssign, node);
      assign->children()->Push(op->lhs());
      assign->children()->Push(rhs);

      Visit(assign);
    }
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

    Visit(wrap);

  } else if (op->subtype() == UnOp::kNot) {
    AddInstruction(new HIRNot(GetValue(op->lhs())));
  } else {
    UNEXPECTED
  }

  return node;
}


AstNode* HIR::VisitBinOp(AstNode* node) {
  BinOp* op = BinOp::Cast(node);

  AddInstruction(new HIRBinOp(
        op->subtype(),
        GetValue(node->lhs()),
        GetValue(node->rhs())));

  return node;
}

} // namespace internal
} // namespace candor
