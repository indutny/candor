#ifndef _SRC_HIR_H_
#define _SRC_HIR_H_

#include "hir-instructions.h"
#include "hir-instructions-inl.h"
#include "heap.h" // Heap
#include "heap-inl.h" // Heap
#include "root.h" // Root
#include "ast.h" // AstNode
#include "scope.h" // ScopeSlot
#include "visitor.h" // Visitor
#include "zone.h" // ZoneObject
#include "utils.h" // PrintBuffer
#include <stdint.h> // int32_t

namespace candor {
namespace internal {

// Forward declaration
class HIRGen;
class HIRBlock;
class LBlock;

typedef ZoneList<HIRBlock*> HIRBlockList;

class HIREnvironment : public ZoneObject {
 public:
  HIREnvironment(int stack_slots);
  void Copy(HIREnvironment* from);

  inline HIRInstruction* At(int i);
  inline HIRPhi* PhiAt(int i);
  inline void Set(int i, HIRInstruction* value);
  inline void SetPhi(int i, HIRPhi* value);

  inline HIRInstruction* At(ScopeSlot* slot);
  inline HIRPhi* PhiAt(ScopeSlot* slot);
  inline void Set(ScopeSlot* slot, HIRInstruction* value);
  inline void SetPhi(ScopeSlot* slot, HIRPhi* value);

  inline ScopeSlot* logic_slot();
  inline int stack_slots();

 protected:
  int stack_slots_;
  ScopeSlot* logic_slot_;
  HIRInstruction** instructions_;
  HIRPhi** phis_;
};

class HIRBlock : public ZoneObject {
 public:
  HIRBlock(HIRGen* g);

  int id;

  HIRInstruction* Assign(ScopeSlot* slot, HIRInstruction* value);
  void Remove(HIRInstruction* instr);
  void PrunePhis();

  inline HIRBlock* AddSuccessor(HIRBlock* b);
  inline HIRInstruction* Add(HIRInstruction::Type type);
  inline HIRInstruction* Add(HIRInstruction::Type type, ScopeSlot* slot);
  inline HIRInstruction* Add(HIRInstruction* instr);
  inline HIRInstruction* Goto(HIRInstruction::Type type, HIRBlock* target);
  inline HIRInstruction* Branch(HIRInstruction::Type type,
                                HIRBlock* t,
                                HIRBlock* f);
  inline HIRInstruction* Return(HIRInstruction::Type type);
  inline bool IsEnded();
  inline bool IsEmpty();
  void MarkPreLoop();
  void MarkLoop();
  inline bool IsLoop();
  inline HIRPhi* CreatePhi(ScopeSlot* slot);

  inline HIREnvironment* env();
  inline void env(HIREnvironment* env);
  inline HIRInstructionList* instructions();
  inline HIRPhiList* phis();
  inline HIRBlock* SuccAt(int i);
  inline HIRBlock* PredAt(int i);
  inline int pred_count();
  inline int succ_count();
  inline LBlock* lir();
  inline void lir(LBlock* lir);

  inline void Print(PrintBuffer* p);

 protected:
  void AddPredecessor(HIRBlock* b);

  HIRGen* g_;

  bool loop_;
  bool ended_;

  HIREnvironment* env_;
  HIRInstructionList instructions_;
  HIRPhiList phis_;

  int pred_count_;
  int succ_count_;
  HIRBlock* pred_[2];
  HIRBlock* succ_[2];

  // Allocator augmentation
  LBlock* lir_;
  int start_id_;
  int end_id_;
};

class BreakContinueInfo : public ZoneObject {
 public:
  BreakContinueInfo(HIRGen* g, HIRBlock* end);

  HIRBlock* GetContinue();
  HIRBlock* GetBreak();

  inline HIRBlockList* continue_blocks();

 private:
  HIRGen* g_;
  HIRBlockList continue_blocks_;
  HIRBlock* brk_;
};

class HIRGen : public Visitor<HIRInstruction> {
 public:
  HIRGen(Heap* heap, AstNode* root);

  void PrunePhis();
  void Replace(HIRInstruction* o, HIRInstruction* n);

  HIRInstruction* VisitFunction(AstNode* stmt);
  HIRInstruction* VisitAssign(AstNode* stmt);
  HIRInstruction* VisitReturn(AstNode* stmt);
  HIRInstruction* VisitValue(AstNode* stmt);
  HIRInstruction* VisitIf(AstNode* stmt);
  HIRInstruction* VisitWhile(AstNode* stmt);
  HIRInstruction* VisitBreak(AstNode* stmt);
  HIRInstruction* VisitContinue(AstNode* stmt);
  HIRInstruction* VisitUnOp(AstNode* stmt);
  HIRInstruction* VisitBinOp(AstNode* stmt);
  HIRInstruction* VisitObjectLiteral(AstNode* stmt);
  HIRInstruction* VisitArrayLiteral(AstNode* stmt);
  HIRInstruction* VisitMember(AstNode* stmt);
  HIRInstruction* VisitDelete(AstNode* stmt);
  HIRInstruction* VisitCall(AstNode* stmt);
  HIRInstruction* VisitTypeof(AstNode* stmt);
  HIRInstruction* VisitSizeof(AstNode* stmt);
  HIRInstruction* VisitKeysof(AstNode* stmt);
  HIRInstruction* VisitClone(AstNode* stmt);

  // Literals

  HIRInstruction* VisitLiteral(AstNode* stmt);
  HIRInstruction* VisitNumber(AstNode* stmt);
  HIRInstruction* VisitNil(AstNode* stmt);
  HIRInstruction* VisitTrue(AstNode* stmt);
  HIRInstruction* VisitFalse(AstNode* stmt);
  HIRInstruction* VisitString(AstNode* stmt);
  HIRInstruction* VisitProperty(AstNode* stmt);

  inline void set_current_block(HIRBlock* b);
  inline void set_current_root(HIRBlock* b);
  inline HIRBlock* current_block();
  inline HIRBlock* current_root();

  inline HIRBlockList* blocks();
  inline HIRBlockList* roots();

  inline HIRInstruction* Add(HIRInstruction::Type type);
  inline HIRInstruction* Add(HIRInstruction::Type type, ScopeSlot* slot);
  inline HIRInstruction* Add(HIRInstruction* instr);
  inline HIRInstruction* Goto(HIRInstruction::Type type, HIRBlock* target);
  inline HIRInstruction* Branch(HIRInstruction::Type type,
                                HIRBlock* t,
                                HIRBlock* f);
  inline HIRInstruction* Return(HIRInstruction::Type type);
  inline HIRBlock* Join(HIRBlock* b1, HIRBlock* b2);
  inline HIRInstruction* Assign(ScopeSlot* slot, HIRInstruction* value);

  inline HIRBlock* CreateBlock(int stack_slots);
  inline HIRBlock* CreateBlock();

  inline HIRInstruction* CreateInstruction(HIRInstruction::Type type);
  inline HIRPhi* CreatePhi(ScopeSlot* slot);
  inline void Print(PrintBuffer* p);
  inline void Print(char* out, int32_t size);

  inline int block_id();
  inline int instr_id();

 private:
  HIRInstructionList work_queue_;

  HIRBlock* current_block_;
  HIRBlock* current_root_;
  BreakContinueInfo* break_continue_info_;

  HIRBlockList roots_;
  HIRBlockList blocks_;
  Root root_;

  int block_id_;
  int instr_id_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_HIR_H_
