#include "hir.h"
#include "hir-inl.h"
#include "hir-instructions.h"
#include "hir-instructions-inl.h"

namespace candor {
namespace internal {

HIRInstruction::HIRInstruction(Type type) :
    id(-1),
    gcm_visited(0),
    type_(type),
    slot_(NULL),
    ast_(NULL),
    lir_(NULL),
    removed_(false),
    pinned_(true),
    representation_(kHoleRepresentation) {
}


HIRInstruction::HIRInstruction(Type type, ScopeSlot* slot) :
    id(-1),
    gcm_visited(0),
    type_(type),
    slot_(slot),
    ast_(NULL),
    lir_(NULL),
    removed_(false),
    pinned_(true),
    representation_(kHoleRepresentation) {
}


void HIRInstruction::Init(HIRGen* g, HIRBlock* block) {
  id = g->instr_id();
  block_ = block;
}


inline void HIRInstruction::CalculateRepresentation() {
  representation_ = kUnknownRepresentation;
}


void HIRInstruction::ReplaceArg(HIRInstruction* o, HIRInstruction* n) {
  HIRInstructionList::Item* head = args()->head();
  HIRInstructionList::Item* next;
  for (; head != NULL; head = next) {
    HIRInstruction* arg = head->value();
    next = head->next();

    if (arg == o) {
      args()->InsertBefore(head, n);
      args()->Remove(head);

      o->RemoveUse(this);
      n->uses()->Push(this);

      break;
    }
  }
}


void HIRInstruction::Remove() {
  removed_ = true;

  HIRInstructionList::Item* head = args()->head();
  for (; head != NULL; head = head->next()) {
    head->value()->RemoveUse(this);
  }
}


void HIRInstruction::RemoveUse(HIRInstruction* i) {
  HIRInstructionList::Item* head = uses()->head();
  HIRInstructionList::Item* next;
  for (; head != NULL; head = next) {
    HIRInstruction* use = head->value();
    next = head->next();

    if (use == i) {
      uses()->Remove(head);
      break;
    }
  }
}


void HIRInstruction::Print(PrintBuffer* p) {
  p->Print("i%d = ", id);

  p->Print("%s", TypeToStr(type_));

  if (type() == HIRInstruction::kLiteral &&
      ast() != NULL && ast()->value() != NULL) {
    p->Print("[");
    p->PrintValue(ast()->value(), ast()->length());
    p->Print("]");
  }

  if (args()->length() == 0) {
    p->Print("\n");
    return;
  }

  HIRInstructionList::Item* head = args()->head();
  p->Print("(");
  for (; head != NULL; head = head->next()) {
    p->Print("i%d", head->value()->id);
    if (head->next() != NULL) p->Print(", ");
  }
  p->Print(")\n");
}


bool HIRInstruction::IsPinned() {
  return pinned_;
}


HIRInstruction* HIRInstruction::Unpin() {
  pinned_ = false;
  return this;
}


HIRInstruction* HIRInstruction::Pin() {
  pinned_ = true;
  return this;
}


HIRPhi::HIRPhi(ScopeSlot* slot) : HIRInstruction(kPhi, slot),
                                  input_count_(0) {
  inputs_[0] = NULL;
  inputs_[1] = NULL;
}


void HIRPhi::Init(HIRGen* g, HIRBlock* b) {
  b->env()->Set(slot(), this);
  b->env()->SetPhi(slot(), this);

  HIRInstruction::Init(g, b);
}


void HIRPhi::CalculateRepresentation() {
  int result = kAnyRepresentation;

  for (int i = 0; i < input_count_; i++) {
    result = result & inputs_[i]->representation();
  }

  representation_ = static_cast<Representation>(result);
}


void HIRPhi::ReplaceArg(HIRInstruction* o, HIRInstruction* n) {
  HIRInstruction::ReplaceArg(o, n);

  for (int i = 0; i < input_count(); i++) {
    if (inputs_[i] == o) inputs_[i] = n;
  }
}


HIRLiteral::HIRLiteral(AstNode::Type type, ScopeSlot* slot) :
    HIRInstruction(kLiteral),
    type_(type),
    root_slot_(slot) {
}


void HIRLiteral::CalculateRepresentation() {
  switch (type_) {
   case AstNode::kNumber:
    representation_ = root_slot_->is_immediate() ?
        kSmiRepresentation : kHeapNumberRepresentation;
    break;
   case AstNode::kString:
   case AstNode::kProperty:
    representation_ = kStringRepresentation;
    break;
   case AstNode::kTrue:
   case AstNode::kFalse:
    representation_ = kBooleanRepresentation;
    break;
   default:
    representation_ = kUnknownRepresentation;
  }
}


HIRFunction::HIRFunction(AstNode* ast) : HIRInstruction(kFunction),
                                         body(NULL),
                                         arg_count(0) {
  ast_ = ast;
}


void HIRFunction::CalculateRepresentation() {
  representation_ = kFunctionRepresentation;
}


void HIRFunction::Print(PrintBuffer* p) {
  p->Print("i%d = Function[b%d]\n", id, body->id);
}


HIRNil::HIRNil() : HIRInstruction(kNil) {
}


HIREntry::HIREntry(int context_slots_) : HIRInstruction(kEntry),
                                         context_slots_(context_slots_) {
}


void HIREntry::Print(PrintBuffer* p) {
  p->Print("i%d = Entry[%d]\n", id, context_slots_);
}


HIRReturn::HIRReturn() : HIRInstruction(kReturn) {
}


HIRIf::HIRIf() : HIRInstruction(kIf) {
}


HIRGoto::HIRGoto() : HIRInstruction(kGoto) {
}


HIRCollectGarbage::HIRCollectGarbage() : HIRInstruction(kCollectGarbage) {
}


HIRGetStackTrace::HIRGetStackTrace() : HIRInstruction(kGetStackTrace) {
}


HIRBinOp::HIRBinOp(BinOp::BinOpType type) : HIRInstruction(kBinOp),
                                            binop_type_(type) {
}


void HIRBinOp::CalculateRepresentation() {
  int left = args()->head()->value()->representation();
  int right = args()->tail()->value()->representation();
  int res;

  if (BinOp::is_binary(binop_type_)) {
    res = kSmiRepresentation;
  } else if (BinOp::is_logic(binop_type_)) {
    res = kBooleanRepresentation;
  } else if (BinOp::is_math(binop_type_)) {
    if (binop_type_ != BinOp::kAdd) {
      res = kNumberRepresentation;
    } else if ((left | right) & kStringRepresentation) {
      // "123" + any, or any + "123"
      res = kStringRepresentation;
    } else {
      int mask = kSmiRepresentation |
                 kHeapNumberRepresentation |
                 kNilRepresentation;
      res = left & right & mask;
    }
  } else {
    res = kUnknownRepresentation;
  }

  representation_ = static_cast<Representation>(res);
}


HIRLoadContext::HIRLoadContext(ScopeSlot* slot)
    : HIRInstruction(kLoadContext),
      context_slot_(slot) {
}


HIRStoreContext::HIRStoreContext(ScopeSlot* slot) :
    HIRInstruction(kStoreContext),
    context_slot_(slot) {
}


void HIRStoreContext::CalculateRepresentation() {
  // Basically store property returns it's first argument
  assert(args()->length() == 1);
  representation_ = args()->tail()->value()->representation();
}


HIRLoadProperty::HIRLoadProperty() : HIRInstruction(kLoadProperty) {
}


HIRStoreProperty::HIRStoreProperty() : HIRInstruction(kStoreProperty) {
}


void HIRStoreProperty::CalculateRepresentation() {
  // Basically store property returns it's first argument
  assert(args()->length() == 3);
  representation_ = args()->head()->value()->representation();
}


HIRDeleteProperty::HIRDeleteProperty() : HIRInstruction(kDeleteProperty) {
}


HIRAllocateObject::HIRAllocateObject() : HIRInstruction(kAllocateObject) {
}


void HIRAllocateObject::CalculateRepresentation() {
  representation_ = kObjectRepresentation;
}


HIRAllocateArray::HIRAllocateArray() : HIRInstruction(kAllocateArray) {
}


void HIRAllocateArray::CalculateRepresentation() {
  representation_ = kArrayRepresentation;
}


HIRLoadArg::HIRLoadArg() : HIRInstruction(kLoadArg) {
}


HIRLoadVarArg::HIRLoadVarArg() : HIRInstruction(kLoadVarArg) {
}


void HIRLoadVarArg::CalculateRepresentation() {
  representation_ = kArrayRepresentation;
}


HIRStoreArg::HIRStoreArg() : HIRInstruction(kStoreArg) {
}


HIRStoreVarArg::HIRStoreVarArg() : HIRInstruction(kStoreVarArg) {
}


HIRAlignStack::HIRAlignStack() : HIRInstruction(kAlignStack) {
}


HIRCall::HIRCall() : HIRInstruction(kCall) {
}


HIRKeysof::HIRKeysof() : HIRInstruction(kKeysof) {
}


void HIRKeysof::CalculateRepresentation() {
  representation_ = kArrayRepresentation;
}


HIRSizeof::HIRSizeof() : HIRInstruction(kSizeof) {
}


void HIRSizeof::CalculateRepresentation() {
  representation_ = kSmiRepresentation;
}


HIRTypeof::HIRTypeof() : HIRInstruction(kTypeof) {
}


void HIRTypeof::CalculateRepresentation() {
  representation_ = kStringRepresentation;
}


HIRClone::HIRClone() : HIRInstruction(kClone) {
}


void HIRClone::CalculateRepresentation() {
  representation_ = kObjectRepresentation;
}

} // namespace internal
} // namespace candor
