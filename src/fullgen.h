#ifndef _SRC_FULLGEN_H_
#define _SRC_FULLGEN_H_

#include "visitor.h"
#include "root.h"
#include "ast.h" // AstNode, FunctionLiteral
#include "zone.h" // ZoneObject
#include "utils.h" // List

#include <assert.h> // assert
#include <stdint.h> // uint32_t

namespace candor {
namespace internal {

// Forward declaration
class Heap;
class SourceMap;
class ScopeSlot;
class Fullgen;
class FInstruction;
class FFunction;
class FLabel;
class FOperand;

typedef ZoneList<FOperand*> FOperandList;

class FOperand : public ZoneObject {
 public:
  enum Type {
    kStack,
    kContext,
    kRegister
  };

  FOperand(Type type, int index, int depth) : type_(type),
                                              index_(index),
                                              depth_(depth) {
  }

  void Print(PrintBuffer* p);

 protected:
  Type type_;
  int index_;
  int depth_;
};

class FStackSlot : public FOperand {
 public:
  FStackSlot(int index) : FOperand(kStack, index, -1) {
  }
};

class FContextSlot : public FOperand {
 public:
  FContextSlot(int index, int depth) : FOperand(kContext, index, depth) {
  }
};

class FRegister : public FOperand {
 public:
  FRegister(int index) : FOperand(kRegister, index, -1) {
  }
};

class FScopedSlot {
 public:
  FScopedSlot(Fullgen* f);
  ~FScopedSlot();

  inline FOperand* operand();
  inline FOperand* operator&() { return operand(); }

 protected:
  Fullgen* f_;
  FOperand* operand_;
};

// Generates non-optimized code by visiting each node in AST tree in-order
class Fullgen : public Visitor<FInstruction> {
 public:
  Fullgen(Heap* heap);

  void Generate(AstNode* ast);

  FInstruction* Visit(AstNode* node);

  FInstruction* VisitFunction(AstNode* stmt);
  FInstruction* VisitCall(AstNode* stmt);
  FInstruction* VisitAssign(AstNode* stmt);

  FInstruction* VisitValue(AstNode* node);

  FInstruction* VisitLiteral(AstNode* node);
  FInstruction* VisitNumber(AstNode* node);
  FInstruction* VisitNil(AstNode* node);
  FInstruction* VisitTrue(AstNode* node);
  FInstruction* VisitFalse(AstNode* node);
  FInstruction* VisitString(AstNode* node);
  FInstruction* VisitProperty(AstNode* node);

  FInstruction* VisitIf(AstNode* node);
  FInstruction* VisitWhile(AstNode* node);

  FInstruction* VisitMember(AstNode* node);
  FInstruction* VisitObjectLiteral(AstNode* node);
  FInstruction* VisitArrayLiteral(AstNode* node);

  FInstruction* VisitReturn(AstNode* node);
  FInstruction* VisitClone(AstNode* node);
  FInstruction* VisitDelete(AstNode* node);
  FInstruction* VisitBreak(AstNode* node);
  FInstruction* VisitContinue(AstNode* node);

  FInstruction* VisitTypeof(AstNode* node);
  FInstruction* VisitSizeof(AstNode* node);
  FInstruction* VisitKeysof(AstNode* node);

  FInstruction* VisitUnOp(AstNode* node);
  FInstruction* VisitBinOp(AstNode* node);

  inline void Print(char* out, int32_t size);
  void Print(PrintBuffer* p);

  inline FInstruction* Add(FInstruction* instr);
  inline FOperand* CreateOperand(ScopeSlot* slot);

  inline void EmptySlots();
  inline FOperand* GetSlot();
  inline void ReleaseSlot(FOperand* slot);
  inline FInstruction* GetNumber(uint64_t i);

  inline int instr_id();

  inline FFunction* current_function();
  inline void set_current_function(FFunction* current_function);

  inline SourceMap* source_map();

 private:
  Heap* heap_;
  Root root_;

  ZoneList<FFunction*> work_queue_;
  ZoneList<FInstruction*> instructions_;

  int instr_id_;
  FFunction* current_function_;
  FLabel* loop_start_;
  FLabel* loop_end_;

  int stack_index_;
  FOperandList free_slots_;

  SourceMap* source_map_;

  friend class FScopedSlot;
};

} // namespace internal
} // namespace candor

#endif // _SRC_FULLGEN_H_
