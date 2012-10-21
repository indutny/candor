#ifndef _SRC_HIR_H_
#define _SRC_HIR_H_

#include "hir-instructions.h"
#include "hir-instructions-inl.h"
#include "heap.h" // Heap
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
class Gen;
class Block;

typedef ZoneList<Block*> BlockList;

class Environment : public ZoneObject {
 public:
  Environment(int stack_slots);
  void Copy(Environment* from);

  inline Instruction* At(int i);
  inline Phi* PhiAt(int i);
  inline void Set(int i, Instruction* value);
  inline void SetPhi(int i, Phi* value);

  inline Instruction* At(ScopeSlot* slot);
  inline Phi* PhiAt(ScopeSlot* slot);
  inline void Set(ScopeSlot* slot, Instruction* value);
  inline void SetPhi(ScopeSlot* slot, Phi* value);

  inline int stack_slots();

 protected:
  int stack_slots_;
  Instruction** instructions_;
  Phi** phis_;
};

class Block : public ZoneObject {
 public:
  Block(Gen* g);

  int id;

  Instruction* Assign(ScopeSlot* slot, Instruction* value);
  void Replace(Instruction* o, Instruction* n);
  void Remove(Instruction* instr);
  void PrunePhis();

  inline Block* AddSuccessor(Block* b);
  inline Instruction* Add(InstructionType type);
  inline Instruction* Add(InstructionType type, ScopeSlot* slot);
  inline Instruction* Add(Instruction* instr);
  inline Instruction* Goto(InstructionType type, Block* target);
  inline Instruction* Branch(InstructionType type, Block* t, Block* f);
  inline Instruction* Return(InstructionType type);
  inline bool IsEnded();
  inline bool IsEmpty();
  inline Phi* CreatePhi(ScopeSlot* slot);

  inline Environment* env();
  inline void env(Environment* env);

  inline void Print(PrintBuffer* p);

 protected:
  void AddPredecessor(Block* b);

  Gen* g_;

  bool loop_;
  bool ended_;

  Environment* env_;
  InstructionList instructions_;
  PhiList phis_;

  // Allocator augmentation
  InstructionList live_gen_;
  InstructionList live_kill_;
  InstructionList live_in_;
  InstructionList live_out_;

  int pred_count_;
  int succ_count_;
  Block* pred_[2];
  Block* succ_[2];
};

class Gen : public Visitor<Instruction> {
 public:
  Gen(Heap* heap, AstNode* root);
  void PrunePhis();

  Instruction* VisitFunction(AstNode* stmt);
  Instruction* VisitAssign(AstNode* stmt);
  Instruction* VisitReturn(AstNode* stmt);
  Instruction* VisitValue(AstNode* stmt);
  Instruction* VisitIf(AstNode* stmt);

  // Literals

  Instruction* VisitLiteral(AstNode* stmt);
  Instruction* VisitNumber(AstNode* stmt);
  Instruction* VisitNil(AstNode* stmt);
  Instruction* VisitTrue(AstNode* stmt);
  Instruction* VisitFalse(AstNode* stmt);
  Instruction* VisitString(AstNode* stmt);
  Instruction* VisitProperty(AstNode* stmt);

  inline void set_current_block(Block* b);
  inline void set_current_root(Block* b);
  inline Block* current_block();
  inline Block* current_root();
  inline Instruction* Add(InstructionType type);
  inline Instruction* Add(InstructionType type, ScopeSlot* slot);
  inline Instruction* Add(Instruction* instr);
  inline Instruction* Goto(InstructionType type, Block* target);
  inline Instruction* Branch(InstructionType type, Block* t, Block* f);
  inline Instruction* Return(InstructionType type);
  inline Block* Join(Block* b1, Block* b2);
  inline Instruction* Assign(ScopeSlot* slot, Instruction* value);

  inline Block* CreateBlock(int stack_slots);
  inline Block* CreateBlock();

  inline Instruction* CreateInstruction(InstructionType type);
  inline Phi* CreatePhi(ScopeSlot* slot);
  inline void Print(PrintBuffer* p);
  inline void Print(char* out, int32_t size);

  inline int block_id();
  inline int instr_id();

 private:
  InstructionList work_queue_;
  BlockList roots_;

  Block* current_block_;
  Block* current_root_;
  BlockList blocks_;
  Root root_;

  int block_id_;
  int instr_id_;
};

} // namespace hir
} // namespace internal
} // namespace candor

#endif // _SRC_HIR_H_
