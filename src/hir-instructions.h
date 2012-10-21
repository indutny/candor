#ifndef _SRC_HIR_INSTRUCTIONS_H_
#define _SRC_HIR_INSTRUCTIONS_H_

#include "ast.h" // AstNode
#include "scope.h" // ScopeSlot
#include "zone.h" // Zone, ZoneList
#include "utils.h" // PrintBuffer

namespace candor {
namespace internal {
namespace hir {

// Forward declarations
class Gen;
class Block;
class Instruction;
class Phi;

typedef ZoneList<Instruction*> InstructionList;
typedef ZoneList<Phi*> PhiList;

#define HIR_INSTRUCTION_TYPES(V) \
    V(Nop) \
    V(Nil) \
    V(Entry) \
    V(Return) \
    V(Function) \
    V(LoadContext) \
    V(StoreContext) \
    V(LoadProperty) \
    V(StoreProperty) \
    V(DeleteProperty) \
    V(If) \
    V(While) \
    V(Literal) \
    V(Goto) \
    V(Not) \
    V(BinOp) \
    V(Typeof) \
    V(Sizeof) \
    V(Keysof) \
    V(Call) \
    V(CollectGarbage) \
    V(GetStackTrace) \
    V(AllocateObject) \
    V(CloneObject) \
    V(AllocateArray) \
    V(Phi)

#define HIR_INSTRUCTION_ENUM(I) \
    k##I,

enum InstructionType {
  HIR_INSTRUCTION_TYPES(HIR_INSTRUCTION_ENUM)
  kNone
};

#undef HIR_INSTRUCTION_ENUM

class Instruction : public ZoneObject {
 public:
  Instruction(Gen* g, Block* block, InstructionType type);
  Instruction(Gen* g, Block* block, InstructionType type, ScopeSlot* slot);

  int id;

  void ReplaceArg(Instruction* o, Instruction* n);
  void RemoveUse(Instruction* i);

  inline Instruction* AddArg(InstructionType type);
  inline Instruction* AddArg(Instruction* instr);
  inline bool Is(InstructionType type);
  inline void Remove();
  inline bool IsRemoved();
  virtual void Print(PrintBuffer* p);
  inline const char* TypeToStr(InstructionType type);

  inline Block* block();
  inline ScopeSlot* slot();
  inline void slot(ScopeSlot* slot);
  inline AstNode* ast();
  inline void ast(AstNode* ast);
  inline InstructionList* args();
  inline InstructionList* uses();

 protected:
  Gen* g_;
  Block* block_;
  InstructionType type_;
  ScopeSlot* slot_;
  AstNode* ast_;

  bool removed_;

  InstructionList args_;
  InstructionList uses_;

  Instruction* prev_;
  Instruction* next_;
};

class Phi : public Instruction {
 public:
  Phi(Gen* g, Block* block, ScopeSlot* slot);

  inline void AddInput(Instruction* instr);
  inline Instruction* InputAt(int i);
  inline void Nilify();

  static inline Phi* Cast(Instruction* instr);

  inline int input_count();

 private:
  int input_count_;
  Instruction* inputs_[2];
};

class Function : public Instruction {
 public:
  Function(Gen* g, Block* block, AstNode* ast);

  Block* body;

  static inline Function* Cast(Instruction* instr);
};

} // namespace hir
} // namespace internal
} // namespace candor

#endif // _SRC_HIR_INSTRUCTIONS_H_
