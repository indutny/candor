#ifndef _SRC_HIR_H_
#define _SRC_HIR_H_

#include "visitor.h" // Visitor
#include "root.h" // Root
#include "zone.h" // ZoneObject
#include "utils.h" // List

// High-level intermediate representation of source code

namespace candor {
namespace internal {

// Forward declarations
class HIR;
class HIRValue;
class HIRInstruction;
class Heap;
class ScopeSlot;
class AstNode;

// CFG Block
class HIRBasicBlock : public ZoneObject {
 public:
  typedef List<HIRInstruction*, ZoneObject> InstructionList;
  typedef List<HIRValue*, ZoneObject> ValueList;

  HIRBasicBlock(HIR* hir);

  // Add value for generating PHIs later
  void AddValue(HIRValue* value);

  // Connect block with others
  void AddPredecessor(HIRBasicBlock* block);
  void AddSuccessor(HIRBasicBlock* block);

  inline HIR* hir() { return hir_; }
  inline ValueList* values() { return &values_; }
  inline InstructionList* instructions() { return &instructions_; }
  inline HIRBasicBlock** predecessors() { return predecessors_; }
  inline HIRBasicBlock** successors() { return successors_; }
  inline int predecessors_count() { return predecessors_count_; }
  inline int successors_count() { return successors_count_; }

 private:
  HIR* hir_;

  ValueList values_;
  InstructionList instructions_;

  HIRBasicBlock* predecessors_[2];
  HIRBasicBlock* successors_[2];
  int predecessors_count_;
  int successors_count_;
};

// SSA Value
class HIRValue : public ZoneObject {
 public:
  HIRValue(HIRBasicBlock* block, ScopeSlot* slot);

  inline HIRBasicBlock* block() { return block_; }
  inline ScopeSlot* slot() { return slot_; }

 private:
  HIRBasicBlock* block_;
  ScopeSlot* slot_;
};

class HIR : public Visitor {
 public:
  HIR(Heap* heap, AstNode* node);

  // Creating blocks and values
  HIRValue* CreateValue(ScopeSlot* slot);
  HIRValue* GetValue(ScopeSlot* slot);
  HIRBasicBlock* CreateBlock();

  // Working with instructions in the current block
  void AddInstruction(HIRInstruction* instr);
  HIRValue* GetLastInstructionResult();

  // Various visiting functions
  AstNode* VisitFunction(AstNode* stmt);
  AstNode* VisitCall(AstNode* stmt);
  AstNode* VisitAssign(AstNode* stmt);

  AstNode* VisitValue(AstNode* node);

  void VisitRootValue(AstNode* node);

  inline AstNode* VisitNumber(AstNode* node) {
    VisitRootValue(node);
    return node;
  }
  inline AstNode* VisitNil(AstNode* node) {
    VisitRootValue(node);
    return node;
  }
  inline AstNode* VisitTrue(AstNode* node) {
    VisitRootValue(node);
    return node;
  }
  inline AstNode* VisitFalse(AstNode* node) {
    VisitRootValue(node);
    return node;
  }
  inline AstNode* VisitString(AstNode* node) {
    VisitRootValue(node);
    return node;
  }
  inline AstNode* VisitProperty(AstNode* node) {
    VisitRootValue(node);
    return node;
  }

  AstNode* VisitIf(AstNode* node);
  AstNode* VisitWhile(AstNode* node);

  AstNode* VisitMember(AstNode* node);
  AstNode* VisitObjectLiteral(AstNode* node);
  AstNode* VisitArrayLiteral(AstNode* node);

  AstNode* VisitReturn(AstNode* node);
  AstNode* VisitClone(AstNode* node);
  AstNode* VisitDelete(AstNode* node);
  AstNode* VisitBreak(AstNode* node);
  AstNode* VisitContinue(AstNode* node);

  AstNode* VisitTypeof(AstNode* node);
  AstNode* VisitSizeof(AstNode* node);
  AstNode* VisitKeysof(AstNode* node);

  AstNode* VisitUnOp(AstNode* node);
  AstNode* VisitBinOp(AstNode* node);

  inline HIRBasicBlock* current_block() { return current_block_; }
  inline void set_current_block(HIRBasicBlock* block) {
    current_block_ = block;
  }

  inline Root* root() { return &root_; }

 private:
  HIRBasicBlock* current_block_;
  Root root_;

  friend class HIRValue;
};

} // namespace internal
} // namespace candor

#endif // _SRC_HIR_H_
