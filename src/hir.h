#ifndef _SRC_HIR_H_
#define _SRC_HIR_H_

#include "hir-instructions.h" // HIRInstructions

#include "visitor.h" // Visitor
#include "root.h" // Root
#include "zone.h" // ZoneObject
#include "utils.h" // List

// High-level intermediate representation of source code

namespace candor {
namespace internal {

// Forward declarations
class HIR;
class HIRPhi;
class HIRValue;
class LIROperand;
class Heap;
class ScopeSlot;
class AstNode;
class RelocationInfo;

typedef ZoneList<HIRValue*> HIRValueList;
typedef ZoneList<HIRPhi*> HIRPhiList;
typedef ZoneList<HIRInstruction*> HIRInstructionList;

// CFG Block
class HIRBasicBlock : public ZoneObject {
 public:
  HIRBasicBlock(HIR* hir);

  // Add value for generating PHIs later
  void AddValue(HIRValue* value);

  // NOTE: Called automatically, do not call by-hand!
  void AddPredecessor(HIRBasicBlock* block);

  // Connect block with others
  void AddSuccessor(HIRBasicBlock* block);
  void Goto(HIRBasicBlock* block);

  // Debug printing
  void Print(PrintBuffer* p);

  // Various ancestors
  inline HIR* hir() { return hir_; }
  inline HIRValueList* values() { return &values_; }
  inline HIRPhiList* phis() { return &phis_; }
  inline HIRInstructionList* instructions() { return &instructions_; }
  inline HIRBasicBlock** predecessors() { return predecessors_; }
  inline HIRBasicBlock** successors() { return successors_; }
  inline int predecessors_count() { return predecessors_count_; }
  inline int successors_count() { return successors_count_; }

  // List of relocation (JIT assembly helper)
  inline ZoneList<RelocationInfo*>* uses() { return &uses_; }

  inline bool finished() { return finished_; }
  inline void finished(bool finished) { finished_ = finished; }

  inline int id() { return id_; }

  void MarkPrinted();
  bool IsPrintable();

 private:
  HIR* hir_;

  HIRValueList values_;
  HIRPhiList phis_;
  HIRInstructionList instructions_;

  HIRBasicBlock* predecessors_[2];
  HIRBasicBlock* successors_[2];
  int predecessors_count_;
  int successors_count_;

  ZoneList<RelocationInfo*> uses_;

  bool finished_;

  int id_;
};

// SSA Value
class HIRValue : public ZoneObject {
 public:
  enum ValueType {
    kNormal,
    kPhi
  };

  struct LiveRange {
    int start;
    int end;
  };

  HIRValue(HIRBasicBlock* block);
  HIRValue(HIRBasicBlock* block, ScopeSlot* slot);

  void Init();

  inline bool is_phi() { return type_ == kPhi; }

  inline HIRBasicBlock* block() { return block_; }
  inline HIRInstructionList* uses() { return &uses_; }
  inline HIRBasicBlock* current_block() { return current_block_; }
  inline void current_block(HIRBasicBlock* current_block) {
    current_block_ = current_block;
  }

  inline HIRValue* prev_def() { return prev_def_; };
  inline void prev_def(HIRValue* prev_def) { prev_def_ = prev_def; };
  inline HIRValueList* next_defs() { return &next_defs_; };

  // LIR helpers
  inline LiveRange* live_range() { return &live_range_; }
  inline LIROperand* operand() { return operand_; }
  inline void operand(LIROperand* operand) { operand_ = operand; }

  inline ScopeSlot* slot() { return slot_; }

  inline int id() { return id_; }

  // Debug printing
  void Print(PrintBuffer* p);

 protected:
  ValueType type_;

  // Block where variable was defined
  HIRBasicBlock* block_;

  // Variable uses
  HIRInstructionList uses_;

  // Block where it is used now (needed for Phi construction)
  HIRBasicBlock* current_block_;

  HIRValue* prev_def_;
  HIRValueList next_defs_;

  // Used in lir.h
  LiveRange live_range_;
  LIROperand* operand_;

  ScopeSlot* slot_;

  int id_;
};

// Phi
class HIRPhi : public HIRValue {
 public:
  HIRPhi(HIRBasicBlock* block, HIRValue* value);

  void Print(PrintBuffer* p);

  static inline HIRPhi* Cast(HIRValue* value) {
    return reinterpret_cast<HIRPhi*>(value);
  }
  inline HIRValueList* inputs() { return &inputs_; }
 private:
  HIRValueList inputs_;
};

// Just a wrapper, see HIR::HIR(...)
class HIRFunction : public ZoneObject {
 public:
  HIRFunction(AstNode* node, HIRBasicBlock* block) : node_(node),
                                                     block_(block) {
  }

  inline AstNode* node() { return node_; }
  inline HIRBasicBlock* block() { return block_; }

 private:
  AstNode* node_;
  HIRBasicBlock* block_;
};

class HIR : public Visitor {
 public:
  typedef HashMap<NumberKey, int, ZoneObject> PrintMap;

  HIR(Heap* heap, AstNode* node);

  // Creating blocks and values
  HIRValue* FindPredecessorValue(ScopeSlot* slot);
  HIRValue* CreateValue(ScopeSlot* slot);
  HIRValue* CreateValue(HIRBasicBlock* block, ScopeSlot* slot);
  HIRValue* CreateValue(HIRBasicBlock* block);

  HIRValue* GetValue(ScopeSlot* slot);
  HIRBasicBlock* CreateBlock();

  // Creates a block
  HIRBasicBlock* CreateJoin(HIRBasicBlock* left, HIRBasicBlock* right);

  // Working with instructions in the current block
  void AddInstruction(HIRInstruction* instr);
  void Finish(HIRInstruction* instr);

  // Visit node and get last instruction's result
  HIRValue* GetValue(AstNode* node);

  // Prints CFG into buffer (debug purposes only)
  void Print(char* buffer, uint32_t size);

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

  void VisitGenericObject(AstNode* node);

  inline AstNode* VisitObjectLiteral(AstNode* node) {
    VisitGenericObject(node);
    return node;
  }
  inline AstNode* VisitArrayLiteral(AstNode* node) {
    VisitGenericObject(node);
    return node;
  }

  AstNode* VisitIf(AstNode* node);
  AstNode* VisitWhile(AstNode* node);

  AstNode* VisitMember(AstNode* node);

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

  inline ZoneList<HIRBasicBlock*>* roots() { return &roots_; }
  inline HIRBasicBlock* root_block() { return root_block_; }
  inline HIRBasicBlock* current_block() { return current_block_; }
  inline void set_current_block(HIRBasicBlock* block) {
    current_block_ = block;
  }

  inline Root* root() { return &root_; }

  inline HIRValueList* values() { return &values_; }
  inline HIRPhiList* phis() { return &phis_; }

  inline HIRInstruction* first_instruction() { return first_instruction_; }
  inline void first_instruction(HIRInstruction* first_instruction) {
    first_instruction_ = first_instruction;
  }

  inline HIRInstruction* last_instruction() { return last_instruction_; }
  inline void last_instruction(HIRInstruction* last_instruction) {
    last_instruction_ = last_instruction;
  }

  inline int get_block_index() { return block_index_++; }
  inline int get_variable_index() { return variable_index_++; }

  // NOTE: Odd instructions are moves,
  // indexing starts from 1
  inline int get_instruction_index() { return instruction_index_ += 2; }

  inline void print_map(PrintMap* print_map) { print_map_ = print_map; }
  inline PrintMap* print_map() { return print_map_; }

 private:
  ZoneList<HIRBasicBlock*> roots_;

  HIRBasicBlock* root_block_;
  HIRBasicBlock* current_block_;
  Root root_;

  HIRValueList values_;
  HIRPhiList phis_;
  HIRInstruction* first_instruction_;
  HIRInstruction* last_instruction_;

  // debugging indexes (and for liveness-range calculations)
  int block_index_;
  int variable_index_;
  int instruction_index_;

  ZoneList<HIRFunction*> work_list_;

  PrintMap* print_map_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_HIR_H_
