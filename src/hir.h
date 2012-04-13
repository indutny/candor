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
class HIRLoopShuffle;
class HIRLoopStart;
class LIROperand;
class Heap;
class ScopeSlot;
class AstNode;
class Masm;
class RelocationInfo;

typedef ZoneList<HIRValue*> HIRValueList;
typedef ZoneList<HIRPhi*> HIRPhiList;
typedef ZoneList<HIRInstruction*> HIRInstructionList;

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

  // NOTE: Called automatically, do not call by-hand!
  void AddPredecessor(HIRBasicBlock* block);

  // Connect block with others
  void AddSuccessor(HIRBasicBlock* block);
  void Goto(HIRBasicBlock* block);

  // Lift values from `block` and add phis where needed
  void LiftValues(HIRBasicBlock* block,
                  HashMap<NumberKey, int, ZoneObject>* map);

  // Replace all uses (not-definitions!) of variable in
  // this block and it's successors
  void ReplaceVarUse(HIRValue* source, HIRValue* target);

  // Block relations
  bool Dominates(HIRBasicBlock* block);

  // Relocation routines
  void AddUse(RelocationInfo* info);
  void Relocate(Masm* masm);

  // Debug printing
  void Print(PrintBuffer* p);

  // Various ancestors
  inline HIR* hir() { return hir_; }
  inline BlockType type() { return type_; }
  inline bool is_normal() { return type_ == kNormal; }
  inline bool is_loop_start() { return type_ == kLoopStart; }

  inline bool is_enumerated() {
    // Check if node's predecessor is dominated by node
    // (node loops to itself)
    if (enumerated_ >= 1 && predecessors_count() == 2) {
      if (predecessors()[1]->Dominates(this) ||
          predecessors()[0]->Dominates(this)) {
        return enumerated_ == 1;
      }
    }
    return enumerated_ == predecessors_count();
  }
  inline void enumerate() { ++enumerated_; }
  inline void reset_enumerate() { enumerated_ = 0; }

  inline HIRValueList* values() { return &values_; }
  inline HIRValueList* inputs() { return &inputs_; }
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

  // List of relocation (JIT assembly helper)
  inline ZoneList<RelocationInfo*>* uses() { return &uses_; }

  // Shuffle to return loop variable invariants back to the start positions
  inline ZoneList<HIRLoopShuffle*>* preshuffle() {
    return preshuffle_;
  }
  inline ZoneList<HIRLoopShuffle*>* postshuffle() {
    return postshuffle_;
  }
  inline void preshuffle(ZoneList<HIRLoopShuffle*>* preshuffle) {
    preshuffle_ = preshuffle;
  }
  inline void postshuffle(ZoneList<HIRLoopShuffle*>* postshuffle) {
    postshuffle_ = postshuffle;
  }

  // Loop associated with current block (for break/continue blocks)
  inline HIRLoopStart* loop_start() { return loop_start_; }
  inline void loop_start(HIRLoopStart* loop_start) { loop_start_ = loop_start; }

  inline bool relocated() { return relocated_; }
  inline void relocated(bool relocated) { relocated_ = relocated; }

  inline bool finished() { return finished_; }
  inline void finished(bool finished) { finished_ = finished; }

  inline int id() { return id_; }

  void MarkPrinted();
  bool IsPrintable();

 protected:
  HIR* hir_;
  BlockType type_;

  // Whether block was enumerated by EnumInstructions() or not
  int enumerated_;

  HIRValueList values_;
  HIRValueList inputs_;
  HIRPhiList phis_;
  HIRInstructionList instructions_;

  HIRBasicBlock* predecessors_[2];
  HIRBasicBlock* successors_[2];
  int predecessors_count_;
  int successors_count_;

  Masm* masm_;
  int relocation_offset_;
  ZoneList<RelocationInfo*> uses_;

  HIRLoopStart* loop_start_;

  ZoneList<HIRLoopShuffle*>* preshuffle_;
  ZoneList<HIRLoopShuffle*>* postshuffle_;

  bool relocated_;
  bool finished_;

  int id_;
};

class HIRLoopShuffle : public ZoneObject {
 public:
  HIRLoopShuffle(HIRValue* value, LIROperand* operand) : value_(value),
                                                         operand_(operand) {
  }

  inline HIRValue* value() { return value_; }
  inline LIROperand* operand() { return operand_; }
  inline void operand(LIROperand* operand) { operand_ = operand; }

 private:
  HIRValue* value_;
  LIROperand* operand_;
};

class HIRLoopStart : public HIRBasicBlock {
 public:
  HIRLoopStart(HIR* hir) : HIRBasicBlock(hir), body_(NULL) {
    type_ = kLoopStart;
    preshuffle(new ZoneList<HIRLoopShuffle*>());
    postshuffle(new ZoneList<HIRLoopShuffle*>());
  }

  static inline HIRLoopStart* Cast(HIRBasicBlock* block) {
    return reinterpret_cast<HIRLoopStart*>(block);
  }

  inline void body(HIRBasicBlock* body) {
    body_ = body;
    body->preshuffle(preshuffle());
    body->postshuffle(postshuffle());
  }
  inline HIRBasicBlock* body() { return body_; }

 private:
  HIRBasicBlock* body_;
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
    inline void Extend(int value) {
      if (start > value) start = value;
      if (end < value) end = value;
    }

    int start;
    int end;
  };

  HIRValue(HIRBasicBlock* block);
  HIRValue(HIRBasicBlock* block, ScopeSlot* slot);
  HIRValue(ValueType type, HIRBasicBlock* block, ScopeSlot* slot);

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
  inline void operand(LIROperand* operand) {
    operand_ = operand;
  }

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

  void AddInput(HIRValue* input);
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

class HIRBreakContinueInfo : public Visitor {
 public:
  HIRBreakContinueInfo(HIR* hir, AstNode* node, HIRLoopStart* ls);
  ~HIRBreakContinueInfo();

  inline AstNode* VisitWhile(AstNode* fn) {
    // Do not count break/continue in child loops
    return fn;
  }

  inline AstNode* VisitFunction(AstNode* fn) {
    // Do not recurse deeper on functions
    return fn;
  }

  void AddBlock(ZoneList<HIRBasicBlock*>* list);

  inline AstNode* VisitBreak(AstNode* fn) {
    break_count_++;
    AddBlock(break_blocks());
    return fn;
  }

  inline AstNode* VisitContinue(AstNode* fn) {
    continue_count_++;
    AddBlock(continue_blocks());
    return fn;
  }

  inline HIR* hir() { return hir_; }

  inline int break_count() { return break_count_; }
  inline int continue_count() { return continue_count_; }

  inline HIRBasicBlock* first_break_block() {
    return break_blocks()->head()->value();
  }
  inline HIRBasicBlock* last_break_block() {
    return break_blocks()->tail()->value();
  }

  inline ZoneList<HIRBasicBlock*>* continue_blocks() {
    return &continue_blocks_;
  }
  inline ZoneList<HIRBasicBlock*>* break_blocks() { return &break_blocks_; }
 private:
  HIR* hir_;
  HIRBreakContinueInfo* previous_;

  int break_count_;
  int continue_count_;

  ZoneList<HIRBasicBlock*> continue_blocks_;
  ZoneList<HIRBasicBlock*> break_blocks_;
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
  void EnumInstructions();

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

  inline ZoneList<HIRBasicBlock*>* roots() { return &roots_; }
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

  inline int get_block_index() { return block_index_++; }
  inline int get_variable_index() { return variable_index_++; }

  inline int get_instruction_index() { return instruction_index_++; }

  inline bool enumerated() { return enumerated_; }
  inline void enumerated(bool enumerated) { enumerated_ = enumerated; }

  inline void print_map(PrintMap* print_map) { print_map_ = print_map; }
  inline PrintMap* print_map() { return print_map_; }

 private:
  ZoneList<HIRBasicBlock*> roots_;

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
  int block_index_;
  int variable_index_;
  int instruction_index_;

  ZoneList<HIRFunction*> work_list_;
  bool enumerated_;

  PrintMap* print_map_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_HIR_H_
