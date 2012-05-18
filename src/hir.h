#ifndef _SRC_HIR_H_
#define _SRC_HIR_H_

#include "hir-instructions.h" // HIRInstructions

#include "visitor.h" // Visitor
#include "root.h" // Root
#include "zone.h" // ZoneObject
#include "utils.h" // List

#include <sys/types.h> // off_t

// High-level intermediate representation of source code

namespace candor {
namespace internal {

// Forward declarations
class HIR;
class HIRBasicBlock;
class HIRPhi;
class HIRValue;
class HIRLoopStart;
class LIROperand;
class Heap;
class ScopeSlot;
class AstNode;
class Masm;

typedef ZoneList<HIRValue*> HIRValueList;
typedef ZoneList<HIRPhi*> HIRPhiList;
typedef ZoneList<HIRInstruction*> HIRInstructionList;
typedef ZoneList<HIRBasicBlock*> HIRBasicBlockList;

// CFG Block
class HIRBasicBlock : public ZoneObject {
 public:
  enum BlockType {
    kNormal,
    kLoopStart
  };

  HIRBasicBlock(HIR* hir);

  // Add value for generating PHIs later
  void AddValue(HIRValue* value);

  // Assigns new dominator or finds closest common of current one and block
  void AssignDominator(HIRBasicBlock* block);

  // NOTE: Called automatically, do not call by-hand!
  void AddPredecessor(HIRBasicBlock* block);

  // Connect block with others
  void AddSuccessor(HIRBasicBlock* block);
  void Goto(HIRBasicBlock* block);

  // Block relations
  bool Dominates(HIRBasicBlock* block);

  // Debug printing
  void Print(PrintBuffer* p);

  // Various ancestors
  inline HIR* hir() { return hir_; }
  inline BlockType type() { return type_; }
  inline bool is_normal() { return type_ == kNormal; }
  inline bool is_loop_start() { return type_ == kLoopStart; }

  inline HIRBasicBlock* dominator() { return dominator_; }
  inline void dominator(HIRBasicBlock* dominator) { dominator_ = dominator; }
  inline HIRBasicBlockList* dominates() { return &dominates_; }

  inline bool is_enumerated() {
    // Check if node's predecessor is dominated by node
    // (node loops to itself)
    if (enumerated_ >= 1 && predecessors_count() == 2) {
      if (this->Dominates(predecessors()[1]) ||
          this->Dominates(predecessors()[0])) {
        return enumerated_ == 1;
      }
    }
    return enumerated_ == predecessors_count();
  }
  inline void enumerate() { ++enumerated_; }
  inline void reset_enumerate() { enumerated_ = 0; }

  inline HIRValueList* values() { return &values_; }
  inline HIRPhiList* phis() { return &phis_; }
  inline HIRInstructionList* instructions() { return &instructions_; }

  inline HIRInstruction* first_instruction() {
    if (instructions()->length() == 0) return NULL;
    return instructions()->head()->value();
  }
  inline HIRInstruction* last_instruction() {
    if (instructions()->length() == 0) return NULL;
    return instructions()->tail()->value();
  }

  inline HIRBasicBlock** predecessors() { return predecessors_; }
  inline HIRBasicBlock** successors() { return successors_; }
  inline int predecessors_count() { return predecessors_count_; }
  inline int successors_count() { return successors_count_; }

  // Loop associated with current block (for break/continue blocks)
  inline HIRLoopStart* loop_start() { return loop_start_; }
  inline void loop_start(HIRLoopStart* loop_start) { loop_start_ = loop_start; }

  inline bool finished() { return finished_; }
  inline void finished(bool finished) { finished_ = finished; }

  inline int id() { return id_; }
  inline void id(int id) { id_ = id; }

  off_t MarkPrinted();
  bool IsPrintable();

 protected:
  HIR* hir_;
  BlockType type_;

  // Whether block was enumerated by Enumerate() or not
  int enumerated_;

  HIRBasicBlock* dominator_;
  HIRBasicBlockList dominates_;

  HIRValueList values_;
  HIRPhiList phis_;
  HIRInstructionList instructions_;

  HIRBasicBlock* predecessors_[2];
  HIRBasicBlock* successors_[2];
  int predecessors_count_;
  int successors_count_;

  Masm* masm_;

  HIRLoopStart* loop_start_;

  bool relocated_;
  bool finished_;

  int id_;
};

class HIRLoopStart : public HIRBasicBlock {
 public:
  HIRLoopStart(HIR* hir) : HIRBasicBlock(hir), end_(NULL) {
    type_ = kLoopStart;
  }

  static inline HIRLoopStart* Cast(HIRBasicBlock* block) {
    return reinterpret_cast<HIRLoopStart*>(block);
  }

  inline void end(HIRBasicBlock* end) {
    assert(end_ == NULL || end_ == end);
    end_ = end;
  }
  inline HIRBasicBlock* end() { return end_; }

 private:
  HIRBasicBlock* end_;
};

// SSA Value
class HIRValue : public ZoneObject {
 public:
  enum ValueType {
    kNormal,
    kPhi
  };

  class LiveRange {
   public:
    inline bool Extend(int value) {
      bool changed = false;

      if (start == -1) {
        start = value;
        end = value;
        return true;
      }

      if (start > value) {
        changed = true;
        start = value;
      }
      if (end < value) {
        changed = true;
        end = value;
      }

      return changed;
    }

    int start;
    int end;
  };

  HIRValue(HIRBasicBlock* block);
  HIRValue(HIRBasicBlock* block, ScopeSlot* slot);
  HIRValue(ValueType type, HIRBasicBlock* block, ScopeSlot* slot);

  void Init();

  // Replace all uses (not-definitions!) of variable
  void Replace(HIRValue* target);

  inline bool is_phi() { return type_ == kPhi; }

  inline HIRBasicBlock* block() { return block_; }
  inline HIRInstructionList* uses() { return &uses_; }
  inline HIRPhiList* phi_uses() { return &phi_uses_; }
  inline HIRBasicBlock* current_block() { return current_block_; }
  inline void current_block(HIRBasicBlock* current_block) {
    current_block_ = current_block;
  }

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
  HIRPhiList phi_uses_;

  // Block where it is used now (needed for Phi construction)
  HIRBasicBlock* current_block_;

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

  void AddInput(HIRValue* input);
  void ReplaceVarUse(HIRValue* source, HIRValue* target);
  void Print(PrintBuffer* p);

  static inline HIRPhi* Cast(HIRValue* value) {
    assert(value->is_phi());
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

class HIRBreakContinueInfo : public Visitor {
 public:
  HIRBreakContinueInfo(HIR* hir, AstNode* node);
  ~HIRBreakContinueInfo();

  enum ListKind {
    kBreakBlocks,
    kContinueBlocks
  };

  HIRBasicBlock* AddBlock(ListKind kind);

  inline AstNode* VisitWhile(AstNode* fn) {
    // Do not count break/continue in child loops
    return fn;
  }

  inline AstNode* VisitFunction(AstNode* fn) {
    // Do not recurse deeper on functions
    return fn;
  }

  inline AstNode* VisitContinue(AstNode* fn) {
    continue_count_++;
    AddBlock(kContinueBlocks);
    return fn;
  }

  inline HIR* hir() { return hir_; }

  inline int continue_count() { return continue_count_; }

  inline HIRBasicBlock* first_break_block() {
    return break_blocks()->head()->value();
  }
  inline HIRBasicBlock* last_break_block() {
    return break_blocks()->tail()->value();
  }
  inline HIRLoopStart* first_continue_block() {
    return HIRLoopStart::Cast(continue_blocks()->head()->value());
  }
  inline HIRLoopStart* last_continue_block() {
    return HIRLoopStart::Cast(continue_blocks()->tail()->value());
  }

  inline HIRBasicBlockList* continue_blocks() {
    return &continue_blocks_;
  }
  inline HIRBasicBlockList* break_blocks() { return &break_blocks_; }

 private:
  HIR* hir_;
  HIRBreakContinueInfo* previous_;

  int continue_count_;

  HIRBasicBlockList continue_blocks_;
  HIRBasicBlockList break_blocks_;
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
  HIRLoopStart* CreateLoopStart();

  // Creates a block
  HIRBasicBlock* CreateJoin(HIRBasicBlock* left, HIRBasicBlock* right);

  // Working with instructions in the current block
  void AddInstruction(HIRInstruction* instr);
  void Finish(HIRInstruction* instr);

  // Go through all blocks in pre-order and enum/link instructions
  void Enumerate();

  // Visit node and get last instruction's result
  HIRValue* GetValue(AstNode* node);
  HIRValue* GetLastResult();

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

  inline HIRBasicBlockList* roots() { return &roots_; }
  inline HIRBasicBlock* root_block() { return root_block_; }
  inline HIRBasicBlock* current_block() { return current_block_; }
  inline void set_current_block(HIRBasicBlock* block) {
    current_block_ = block;
  }

  inline Root* root() { return &root_; }

  inline HIRBreakContinueInfo* break_continue_info() {
    return break_continue_info_;
  }
  inline void break_continue_info(HIRBreakContinueInfo* info) {
    break_continue_info_ = info;
  }

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

  inline int get_variable_index() { return variable_index_++; }

  inline int get_instruction_index() { return instruction_index_++; }

  inline bool enumerated() { return enumerated_; }
  inline void enumerated(bool enumerated) { enumerated_ = enumerated; }

  inline void print_map(PrintMap* print_map) { print_map_ = print_map; }
  inline PrintMap* print_map() { return print_map_; }

 private:
  HIRBasicBlockList roots_;

  HIRBasicBlock* root_block_;
  HIRBasicBlock* current_block_;
  Root root_;

  // Current loop information
  HIRBreakContinueInfo* break_continue_info_;

  HIRValueList values_;
  HIRPhiList phis_;
  HIRInstruction* first_instruction_;
  HIRInstruction* last_instruction_;

  // debugging indexes (and for liveness-range calculations)
  int variable_index_;
  int instruction_index_;

  ZoneList<HIRFunction*> work_list_;
  bool enumerated_;

  PrintMap* print_map_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_HIR_H_
