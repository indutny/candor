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

class Instruction : public ZoneObject {
 public:
  enum Type {
    HIR_INSTRUCTION_TYPES(HIR_INSTRUCTION_ENUM)
    kNone
  };

  Instruction(Gen* g, Block* block, Type type);
  Instruction(Gen* g, Block* block, Type type, ScopeSlot* slot);

  int id;

  void ReplaceArg(Instruction* o, Instruction* n);
  void RemoveUse(Instruction* i);

  inline Instruction* AddArg(Type type);
  inline Instruction* AddArg(Instruction* instr);
  inline bool Is(Type type);
  inline void Remove();
  inline bool IsRemoved();
  virtual void Print(PrintBuffer* p);
  inline const char* TypeToStr(Type type);

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
  Type type_;
  ScopeSlot* slot_;
  AstNode* ast_;

  bool removed_;

  InstructionList args_;
  InstructionList uses_;
};

#undef HIR_INSTRUCTION_ENUM

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

  void Print(PrintBuffer* p);
  static inline Function* Cast(Instruction* instr);
};

} // namespace hir
} // namespace internal
} // namespace candor

#endif // _SRC_HIR_INSTRUCTIONS_H_
