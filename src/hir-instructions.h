#ifndef _SRC_HIR_INSTRUCTIONS_H_
#define _SRC_HIR_INSTRUCTIONS_H_

#include "ast.h" // AstNode
#include "scope.h" // ScopeSlot
#include "zone.h" // Zone, ZoneList
#include "utils.h" // PrintBuffer

namespace candor {
namespace internal {

// Forward declarations
class HIRGen;
class HIRBlock;
class HIRInstruction;
class HIRPhi;
class LInstruction;

typedef ZoneList<HIRInstruction*> HIRInstructionList;
typedef ZoneList<HIRPhi*> HIRPhiList;

#define HIR_INSTRUCTION_TYPES(V) \
    V(Nop) \
    V(Nil) \
    V(Entry) \
    V(Return) \
    V(Function) \
    V(LoadArg) \
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
    V(Clone) \
    V(Call) \
    V(CollectGarbage) \
    V(GetStackTrace) \
    V(AllocateObject) \
    V(AllocateArray) \
    V(Phi)

#define HIR_INSTRUCTION_ENUM(I) \
    k##I,

class HIRInstruction : public ZoneObject {
 public:
  enum Type {
    HIR_INSTRUCTION_TYPES(HIR_INSTRUCTION_ENUM)
    kNone
  };

  HIRInstruction(HIRGen* g, HIRBlock* block, Type type);
  HIRInstruction(HIRGen* g, HIRBlock* block, Type type, ScopeSlot* slot);

  int id;

  void ReplaceArg(HIRInstruction* o, HIRInstruction* n);
  void RemoveUse(HIRInstruction* i);

  inline HIRInstruction* AddArg(Type type);
  inline HIRInstruction* AddArg(HIRInstruction* instr);
  inline bool Is(Type type);
  inline Type type();
  inline void Remove();
  inline bool IsRemoved();
  virtual void Print(PrintBuffer* p);
  inline const char* TypeToStr(Type type);

  inline HIRBlock* block();
  inline ScopeSlot* slot();
  inline void slot(ScopeSlot* slot);
  inline AstNode* ast();
  inline void ast(AstNode* ast);
  inline HIRInstructionList* args();
  inline HIRInstructionList* uses();

  inline HIRInstruction* left();
  inline HIRInstruction* right();
  inline HIRInstruction* third();

  inline LInstruction* lir();
  inline void lir(LInstruction* lir);

 protected:
  HIRGen* g_;
  HIRBlock* block_;
  Type type_;
  ScopeSlot* slot_;
  AstNode* ast_;
  LInstruction* lir_;

  bool removed_;

  HIRInstructionList args_;
  HIRInstructionList uses_;
};

#undef HIR_INSTRUCTION_ENUM

class HIRPhi : public HIRInstruction {
 public:
  HIRPhi(HIRGen* g, HIRBlock* block, ScopeSlot* slot);

  inline void AddInput(HIRInstruction* instr);
  inline HIRInstruction* InputAt(int i);
  inline void Nilify();

  static inline HIRPhi* Cast(HIRInstruction* instr);

  inline int input_count();

 private:
  int input_count_;
  HIRInstruction* inputs_[2];
};

class HIRFunction : public HIRInstruction {
 public:
  HIRFunction(HIRGen* g, HIRBlock* block, AstNode* ast);

  HIRBlock* body;

  void Print(PrintBuffer* p);
  static inline HIRFunction* Cast(HIRInstruction* instr);
};

class HIRLoadArg : public HIRInstruction {
 public:
  HIRLoadArg(HIRGen* g, HIRBlock* block, int index);

  void Print(PrintBuffer* p);
  static inline HIRLoadArg* Cast(HIRInstruction* instr);

 private:
  int index_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_HIR_INSTRUCTIONS_H_
