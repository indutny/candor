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
namespace hir {

// Forward declaration
class HGen;
class HBlock;

typedef ZoneList<HBlock*> HBlockList;

class HEnvironment : public ZoneObject {
 public:
  HEnvironment(int stack_slots);
  void Copy(HEnvironment* from);

  inline HInstruction* At(int i);
  inline HPhi* PhiAt(int i);
  inline void Set(int i, HInstruction* value);
  inline void SetPhi(int i, HPhi* value);

  inline HInstruction* At(ScopeSlot* slot);
  inline HPhi* PhiAt(ScopeSlot* slot);
  inline void Set(ScopeSlot* slot, HInstruction* value);
  inline void SetPhi(ScopeSlot* slot, HPhi* value);

  inline ScopeSlot* logic_slot();
  inline int stack_slots();

 protected:
  int stack_slots_;
  ScopeSlot* logic_slot_;
  HInstruction** instructions_;
  HPhi** phis_;
};

class HBlock : public ZoneObject {
 public:
  HBlock(HGen* g);

  int id;

  HInstruction* Assign(ScopeSlot* slot, HInstruction* value);
  void Remove(HInstruction* instr);
  void PruneHPhis();

  inline HBlock* AddSuccessor(HBlock* b);
  inline HInstruction* Add(HInstruction::Type type);
  inline HInstruction* Add(HInstruction::Type type, ScopeSlot* slot);
  inline HInstruction* Add(HInstruction* instr);
  inline HInstruction* Goto(HInstruction::Type type, HBlock* target);
  inline HInstruction* Branch(HInstruction::Type type, HBlock* t, HBlock* f);
  inline HInstruction* Return(HInstruction::Type type);
  inline bool IsEnded();
  inline bool IsEmpty();
  void MarkLoop();
  inline bool IsLoop();
  inline HPhi* CreatePhi(ScopeSlot* slot);

  inline HEnvironment* env();
  inline void env(HEnvironment* env);

  inline void Print(PrintBuffer* p);

 protected:
  void AddPredecessor(HBlock* b);

  HGen* g_;

  bool loop_;
  bool ended_;

  HEnvironment* env_;
  HInstructionList instructions_;
  HPhiList phis_;

  // Allocator augmentation
  HInstructionList live_gen_;
  HInstructionList live_kill_;
  HInstructionList live_in_;
  HInstructionList live_out_;

  int pred_count_;
  int succ_count_;
  HBlock* pred_[2];
  HBlock* succ_[2];
};

class BreakContinueInfo : public ZoneObject {
 public:
  BreakContinueInfo(HGen* g, HBlock* end);

  HBlock* GetContinue();
  HBlock* GetBreak();

  inline HBlockList* continue_blocks();

 private:
  HGen* g_;
  HBlockList continue_blocks_;
  HBlock* brk_;
};

class HGen : public Visitor<HInstruction> {
 public:
  HGen(Heap* heap, AstNode* root);

  void PruneHPhis();
  void Replace(HInstruction* o, HInstruction* n);

  HInstruction* VisitFunction(AstNode* stmt);
  HInstruction* VisitAssign(AstNode* stmt);
  HInstruction* VisitReturn(AstNode* stmt);
  HInstruction* VisitValue(AstNode* stmt);
  HInstruction* VisitIf(AstNode* stmt);
  HInstruction* VisitWhile(AstNode* stmt);
  HInstruction* VisitBreak(AstNode* stmt);
  HInstruction* VisitContinue(AstNode* stmt);
  HInstruction* VisitUnOp(AstNode* stmt);
  HInstruction* VisitBinOp(AstNode* stmt);
  HInstruction* VisitObjectLiteral(AstNode* stmt);
  HInstruction* VisitArrayLiteral(AstNode* stmt);
  HInstruction* VisitMember(AstNode* stmt);
  HInstruction* VisitDelete(AstNode* stmt);
  HInstruction* VisitCall(AstNode* stmt);
  HInstruction* VisitTypeof(AstNode* stmt);
  HInstruction* VisitSizeof(AstNode* stmt);
  HInstruction* VisitKeysof(AstNode* stmt);

  // Literals

  HInstruction* VisitLiteral(AstNode* stmt);
  HInstruction* VisitNumber(AstNode* stmt);
  HInstruction* VisitNil(AstNode* stmt);
  HInstruction* VisitTrue(AstNode* stmt);
  HInstruction* VisitFalse(AstNode* stmt);
  HInstruction* VisitString(AstNode* stmt);
  HInstruction* VisitProperty(AstNode* stmt);

  inline void set_current_block(HBlock* b);
  inline void set_current_root(HBlock* b);
  inline HBlock* current_block();
  inline HBlock* current_root();
  inline HInstruction* Add(HInstruction::Type type);
  inline HInstruction* Add(HInstruction::Type type, ScopeSlot* slot);
  inline HInstruction* Add(HInstruction* instr);
  inline HInstruction* Goto(HInstruction::Type type, HBlock* target);
  inline HInstruction* Branch(HInstruction::Type type, HBlock* t, HBlock* f);
  inline HInstruction* Return(HInstruction::Type type);
  inline HBlock* Join(HBlock* b1, HBlock* b2);
  inline HInstruction* Assign(ScopeSlot* slot, HInstruction* value);

  inline HBlock* CreateBlock(int stack_slots);
  inline HBlock* CreateBlock();

  inline HInstruction* CreateInstruction(HInstruction::Type type);
  inline HPhi* CreatePhi(ScopeSlot* slot);
  inline void Print(PrintBuffer* p);
  inline void Print(char* out, int32_t size);

  inline int block_id();
  inline int instr_id();

 private:
  HInstructionList work_queue_;
  HBlockList roots_;

  HBlock* current_block_;
  HBlock* current_root_;
  BreakContinueInfo* break_continue_info_;
  HBlockList blocks_;
  Root root_;

  int block_id_;
  int instr_id_;
};

} // namespace hir
} // namespace internal
} // namespace candor

#endif // _SRC_HIR_H_
