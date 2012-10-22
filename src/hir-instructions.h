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
class HGen;
class HBlock;
class HInstruction;
class HPhi;

typedef ZoneList<HInstruction*> HInstructionList;
typedef ZoneList<HPhi*> HPhiList;

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

class HInstruction : public ZoneObject {
 public:
  enum Type {
    HIR_INSTRUCTION_TYPES(HIR_INSTRUCTION_ENUM)
    kNone
  };

  HInstruction(HGen* g, HBlock* block, Type type);
  HInstruction(HGen* g, HBlock* block, Type type, ScopeSlot* slot);

  int id;

  void ReplaceArg(HInstruction* o, HInstruction* n);
  void RemoveUse(HInstruction* i);

  inline HInstruction* AddArg(Type type);
  inline HInstruction* AddArg(HInstruction* instr);
  inline bool Is(Type type);
  inline void Remove();
  inline bool IsRemoved();
  virtual void Print(PrintBuffer* p);
  inline const char* TypeToStr(Type type);

  inline HBlock* block();
  inline ScopeSlot* slot();
  inline void slot(ScopeSlot* slot);
  inline AstNode* ast();
  inline void ast(AstNode* ast);
  inline HInstructionList* args();
  inline HInstructionList* uses();

 protected:
  HGen* g_;
  HBlock* block_;
  Type type_;
  ScopeSlot* slot_;
  AstNode* ast_;

  bool removed_;

  HInstructionList args_;
  HInstructionList uses_;
};

#undef HIR_INSTRUCTION_ENUM

class HPhi : public HInstruction {
 public:
  HPhi(HGen* g, HBlock* block, ScopeSlot* slot);

  inline void AddInput(HInstruction* instr);
  inline HInstruction* InputAt(int i);
  inline void Nilify();

  static inline HPhi* Cast(HInstruction* instr);

  inline int input_count();

 private:
  int input_count_;
  HInstruction* inputs_[2];
};

class HFunction : public HInstruction {
 public:
  HFunction(HGen* g, HBlock* block, AstNode* ast);

  HBlock* body;

  void Print(PrintBuffer* p);
  static inline HFunction* Cast(HInstruction* instr);
};

} // namespace hir
} // namespace internal
} // namespace candor

#endif // _SRC_HIR_INSTRUCTIONS_H_
