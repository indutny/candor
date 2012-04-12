#include "hir.h"
#include "hir-instructions.h"
#include "visitor.h" // Visitor
#include "ast.h" // AstNode
#include "macroassembler.h" // Masm, RelocationInfo
#include "utils.h" // List

#include <stdlib.h> // NULL
#include <stdint.h> // int64_t
#include <assert.h> // assert

#define __STDC_FORMAT_MACROS
#include <inttypes.h> // printf formats for big integers

namespace candor {
namespace internal {

HIRBasicBlock::HIRBasicBlock(HIR* hir) : hir_(hir),
                                         type_(kNormal),
                                         enumerated_(0),
                                         predecessors_count_(0),
                                         successors_count_(0),
                                         masm_(NULL),
                                         relocation_offset_(0),
                                         relocated_(false),
                                         finished_(false),
                                         id_(hir->get_block_index()) {
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

  // Inputs list contain only first added variable
  item = inputs()->head();
  for (; item != NULL; item = item->next()) {
    if (item->value()->slot() == value->slot()) {
      if (value->is_phi()) {
        inputs()->Remove(item);
        item = NULL;
      }
      break;
    }
  }

  if (item == NULL) inputs()->Push(value);
}


void HIRBasicBlock::AddPredecessor(HIRBasicBlock* block) {
  assert(predecessors_count() < 2);
  predecessors()[predecessors_count_++] = block;

  HashMap<NumberKey, int, ZoneObject> map;
  LiftValues(block, &map);
}


void HIRBasicBlock::AddSuccessor(HIRBasicBlock* block) {
  assert(successors_count() < 2);
  successors()[successors_count_++] = block;
  block->AddPredecessor(this);
}


void HIRBasicBlock::LiftValues(HIRBasicBlock* block,
                               HashMap<NumberKey, int, ZoneObject>* map) {
  // Skip loops
  if (map->Get(NumberKey::New(reinterpret_cast<char*>(this)))) return;
  int mark;
  map->Set(NumberKey::New(reinterpret_cast<char*>(this)), &mark);

  HIRValueList::Item* item;
  if (predecessors_count() > 1) {
    // Mark values propagated from first predecessor
    item = inputs()->head();
    for (; item != NULL; item = item->next()) {
      if (item->value() == NULL) continue;

      item->value()->current_block(this);
      item->value()->slot()->hir(item->value());
    }
  }

  // Propagate used values from predecessor to current block
  item = block->values()->head();
  for (; item != NULL; item = item->next()) {
    HIRValue* value = item->value();

    // Prune dead variables
    if (value == NULL ||
        (!value->is_phi() && value->uses()->length() == 0)) {
      continue;
    }

    // If value is already in current block - insert phi!
    if (value->slot()->hir()->current_block() == this &&
        value->slot()->hir() != value) {
      HIRPhi* phi = HIRPhi::Cast(value->slot()->hir());
      if (!phi->is_phi()) {
        phi = new HIRPhi(this, value->slot()->hir());

        // Replace slot's value
        value->slot()->hir(phi);
      }

      phi->AddInput(value);
    } else {
      // And associated with a current block
      value->current_block(this);
      value->slot()->hir(value);

      // Otherwise put value to the list
      AddValue(value);
    }
  }

  if (successors_count() > 0) {
    // If any value was added - propagate them to successors
    for (int i = 0; i < successors_count(); i++) {
      successors()[i]->LiftValues(this, map);
    }
  }

  // If loop detected and there're phis those inputs are defined in this block
  // (i.e. in loop header) - use them as values
  if (instructions()->length() != 0) {
    // Add non-local variable uses to the end of loop
    // to ensure that they will be live after the first loop's pass
    if (instructions()->length() != 0) {
      item = block->values()->head();
      for (; item != NULL; item = item->next()) {
        HIRValue* value = item->value();

        if (value == NULL ||
            (Dominates(value->block()) && value->block() != block)) {
          continue;
        }
        value->uses()->Push(block->last_instruction());
      }
    }

    HIRPhiList::Item* item = phis()->head();
    for (; item != NULL; item = item->next()) {
      HIRPhi* phi = item->value();

      HIRValue* inputs[2] = { phi->inputs()->head()->value(),
                              phi->inputs()->tail()->value() };
      for (int i = 0; i < 2; i++) {
        if (inputs[i]->block() == this) {
          inputs[i]->current_block(this);
          inputs[i]->slot()->hir(inputs[i]);
          break;
        }
      }
    }
  }
}


void HIRBasicBlock::Goto(HIRBasicBlock* block) {
  if (finished()) return;

  // Add goto instruction and finalize block
  HIRGoto* instr = new HIRGoto();
  instructions()->Push(instr);
  instr->Init(this);
  finished(true);

  // Connect graph nodes
  AddSuccessor(block);
}


void HIRBasicBlock::ReplaceVarUse(HIRValue* source, HIRValue* target) {
  if (instructions()->length() == 0) return;

  ZoneList<HIRBasicBlock*> work_list;
  ZoneList<HIRBasicBlock*> cleanup_list;

  work_list.Push(this);

  HIRBasicBlock* block;
  while ((block = work_list.Shift()) != NULL) {
    cleanup_list.Push(block);

    if (target->block()->Dominates(block)) {
      // Replace value use in each instruction
      ZoneList<HIRInstruction*>::Item* item = block->instructions()->head();
      for (; item != NULL; item = item->next()) {
        item->value()->ReplaceVarUse(source, target);
      }
    }

    // Add block's successors to the work list
    for (int i = block->successors_count() - 1; i >= 0; i--) {
      // Skip processed blocks and join blocks that was visited only once
      block->successors()[i]->enumerate();
      if (block->successors()[i]->is_enumerated()) {
        work_list.Unshift(block->successors()[i]);
      }
    }
  }

  while ((block = cleanup_list.Shift()) != NULL) {
    block->reset_enumerate();
  }
}


bool HIRBasicBlock::Dominates(HIRBasicBlock* block) {
  while (block != NULL) {
    if (block == this) return true;
    block = block->predecessors()[0];
  }

  return false;
}


void HIRBasicBlock::AddUse(RelocationInfo* info) {
  if (relocated()) {
    info->target(relocation_offset_);
    masm_->relocation_info_.Push(info);
    return;
  }
  uses()->Push(info);
}


void HIRBasicBlock::Relocate(Masm* masm) {
  if (relocated()) return;
  masm_ = masm;
  relocation_offset_ = masm->offset();
  relocated(true);

  RelocationInfo* block_reloc;
  while ((block_reloc = uses()->Shift()) != NULL) {
    block_reloc->target(masm->offset());
    masm->relocation_info_.Push(block_reloc);
  }
}


bool HIRBasicBlock::IsPrintable() {
  return hir()->print_map()->Get(NumberKey::New(id())) == NULL;
}


void HIRBasicBlock::MarkPrinted() {
  int value;
  hir()->print_map()->Set(NumberKey::New(id()), &value);
}


void HIRBasicBlock::Print(PrintBuffer* p) {
  // Avoid loops and double prints
  MarkPrinted();

  p->Print("[Block#%d", id());

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
      if (item->value()->type() == HIRInstruction::kNop) continue;
      item->value()->Print(p);
    }
  }

  // Print predecessors' ids
  if (predecessors_count() == 2) {
    p->Print("[%d,%d]", predecessors()[0]->id(), predecessors()[1]->id());
  } else if (predecessors_count() == 1) {
    p->Print("[%d]", predecessors()[0]->id());
  } else {
    p->Print("[]");
  }

  p->Print(">*>");

  // Print successors' ids
  if (successors_count() == 2) {
    p->Print("[%d,%d]", successors()[0]->id(), successors()[1]->id());
  } else if (successors_count() == 1) {
    p->Print("[%d]", successors()[0]->id());
  } else {
    p->Print("[]");
  }

  p->Print("]\n\n");

  // Print successors
  if (successors_count() == 2) {
    if (successors()[0]->IsPrintable()) successors()[0]->Print(p);
    if (successors()[1]->IsPrintable()) successors()[1]->Print(p);
  } else if (successors_count() == 1) {
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
  if (inputs()->length() > 1) {
    HIRValue* left = inputs()->head()->value();

    // XXX: Some sort of black magic here, I need to understand how it works
    if (input->is_phi()) {
      HIRPhi* phi_input = HIRPhi::Cast(input);
      if (phi_input->inputs()->length() != 0 &&
          left != phi_input->inputs()->head()->value() &&
          left != phi_input->inputs()->tail()->value()) {
        block()->ReplaceVarUse(input, this);
        return;
      }
    }

    // Remove excessive inputs, use only last redifinition of value
    // (Useful in loop's start block)
    inputs()->Shift();
  }

  if (inputs()->length() == 1) {
    // Ignore duplicate inputs
    if (inputs()->head()->value() == input) return;
  }

  inputs()->Push(input);

  if (inputs()->length() > 1) {
    // Replace all input's uses if Phi appeared in loop
    HIRValueList::Item* item = inputs()->head();
    for (; item != NULL; item = item->next()) {
      block()->ReplaceVarUse(item->value(), this);
    }

    // Ensure that phis are in correct order
    HIRValue* left = inputs()->head()->value();
    HIRValue* right = inputs()->tail()->value();

    if ((!left->block()->Dominates(block()) || left->block() == block()) &&
        right->block()->Dominates(block())) {
      inputs()->Push(inputs()->Shift());
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
                                           prev_def_(NULL),
                                           operand_(NULL) {
  slot_ = new ScopeSlot(ScopeSlot::kStack);
  Init();
}


HIRValue::HIRValue(HIRBasicBlock* block, ScopeSlot* slot)
    : type_(kNormal),
      block_(block),
      current_block_(block),
      prev_def_(NULL),
      operand_(NULL),
      slot_(slot) {
  Init();
}


HIRValue::HIRValue(ValueType type, HIRBasicBlock* block, ScopeSlot* slot)
    : type_(type),
      block_(block),
      current_block_(block),
      prev_def_(NULL),
      operand_(NULL),
      slot_(slot) {
  Init();
}


void HIRValue::Init() {
  block()->AddValue(this);
  id_ = block()->hir()->get_variable_index();

  live_range()->start = -1;
  live_range()->end = -1;
}


void HIRValue::Print(PrintBuffer* p) {
  p->Print("*[%d ", id());
  if (live_range()->start != -1) {
    p->Print("<%d,%d>", live_range()->start, live_range()->end);
  }
  slot()->Print(p);
  p->Print("]");
}


HIR::HIR(Heap* heap, AstNode* node) : Visitor(kPreorder),
                                      root_(heap),
                                      first_instruction_(NULL),
                                      last_instruction_(NULL),
                                      block_index_(0),
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

  EnumInstructions();
}


HIRValue* HIR::FindPredecessorValue(ScopeSlot* slot) {
  assert(current_block() != NULL);

  // Find appropriate value
  HIRValue* previous = slot->hir();
  while (previous != NULL) {
    // Traverse blocks to the root, to check
    // if variable was used in predecessor
    HIRBasicBlock* block = current_block();
    while (block != NULL && previous->block() != block) {
      block = block->predecessors()[0];
    }
    if (block != NULL) break;
    previous = previous->prev_def();
  }

  return previous;
}


HIRValue* HIR::CreateValue(HIRBasicBlock* block, ScopeSlot* slot) {
  HIRValue* value = new HIRValue(block, slot);
  HIRValue* previous = FindPredecessorValue(slot);

  // Link with previous
  if (previous != NULL) {
    value->prev_def(previous);
    previous->next_defs()->Push(value);
  }

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
    if (previous != slot->hir()) {
      // Create slot and link variables
      HIRValue* value = new HIRValue(current_block(), slot);

      // Link with previous
      if (slot->hir() != NULL) {
        value->prev_def(previous);
        previous->next_defs()->Push(value);
      }

      slot->hir(value);

      // Push value to the values list
      values()->Push(value);
    } else {
      slot->hir()->current_block(current_block());
    }
  }

  return slot->hir();
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


void HIR::EnumInstructions() {
  ZoneList<HIRBasicBlock*>::Item* root = roots()->head();
  ZoneList<HIRBasicBlock*> work_list;

  // Add roots to worklist
  for (; root != NULL; root = root->next()) {
    root->value()->enumerate();
    work_list.Push(root->value());
  }

  // Add first instruction
  {
    HIRParallelMove* move = new HIRParallelMove();
    move->Init(NULL);
    move->id(get_instruction_index());
    first_instruction(move);
    last_instruction(move);
  }

  // Process worklist
  HIRBasicBlock* current;
  while ((current = work_list.Shift()) != NULL) {
    // Insert nop instruction in empty blocks
    if (current->instructions()->length() == 0) {
      set_current_block(current);
      AddInstruction(new HIRNop());
    }

    // Prune phis with less than 2 inputs
    HIRPhiList::Item* item = current->phis()->head();
    while (item != NULL) {
      if (item->value()->inputs()->length() < 2) {
        HIRPhiList::Item* next = item->next();
        current->phis()->Remove(item);
        item = next;
      } else {
        item = item->next();
      }
    }

    // Go through all block's instructions, link them together and assign id
    HIRInstructionList::Item* instr = current->instructions()->head();
    for (; instr != NULL; instr = instr->next()) {
      // Insert instruction into linked list
      last_instruction()->next(instr->value());

      instr->value()->prev(last_instruction());
      instr->value()->id(get_instruction_index());
      last_instruction(instr->value());

      // Insert parallel move after each instruction
      HIRParallelMove* move = new HIRParallelMove();
      move->Init(instr->value()->block());

      last_instruction()->next(move);

      move->prev(last_instruction());
      move->id(get_instruction_index());
      last_instruction(move);
    }

    // Add block's successors to the work list
    for (int i = current->successors_count() - 1; i >= 0; i--) {
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
    return CreateValue(root()->Put(new AstNode(AstNode::kNil)));
  } else {
    return GetLastResult();
  }
}


HIRValue* HIR::GetLastResult() {
  return current_block()->instructions()->tail()->value()->GetResult();
}


void HIR::Print(char* buffer, uint32_t size) {
  PrintMap map;

  PrintBuffer p(buffer, size);
  print_map(&map);

  ZoneList<HIRBasicBlock*>::Item* item = roots()->head();
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

    AddInstruction(new HIRReturn(CreateValue(
            root()->Put(new AstNode(AstNode::kNil)))));
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
  } else {
    // TODO: Set error! Incorrect lhs
    abort();
  }

  // Propagate result
  AddInstruction(new HIRNop(rhs));

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
  HIRLoopStart* cond = CreateLoopStart();
  HIRBasicBlock* body = CreateBlock();
  HIRBasicBlock* end = CreateBlock();

  //   entry
  //     |
  //    lhs
  //     | \
  //     |  ^
  //     |  |
  //     | body
  //     |  |
  //     |  ^
  //     | /
  //   branch
  //     |
  //     |
  //    end

  // Create new block and insert condition expression into it
  current_block()->Goto(cond);
  set_current_block(cond);
  HIRBranchBool* branch = new HIRBranchBool(GetValue(node->lhs()),
                                            body,
                                            end);
  Finish(branch);

  // Generate loop's body
  set_current_block(body);
  Visit(node->rhs());

  // And loop it back to condition
  current_block()->Goto(cond);
  cond->body(current_block());

  // Execution will continue in the `end` block
  set_current_block(end);

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

  Visit(fn->variable());
  HIRCall* call = new HIRCall(GetValue(fn->variable()));

  AstList::Item* item = fn->args()->head();
  for (; item != NULL; item = item->next()) {
    call->AddArg(GetValue(item->value()));
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
    result = CreateValue(root()->Put(new AstNode(AstNode::kNil)));
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
  return node;
}


AstNode* HIR::VisitContinue(AstNode* node) {
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
