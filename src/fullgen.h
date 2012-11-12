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
class CodeSpace;
class SourceMap;
class FInstruction;
class FLabel;

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

// Generates non-optimized code by visiting each node in AST tree in-order
class Fullgen : public Visitor<FInstruction> {
 public:
  Fullgen(CodeSpace* space, SourceMap* map);

  void Generate(AstNode* ast);

  FInstruction* Visit(AstNode* node);

  FInstruction* VisitFunction(AstNode* stmt);
  FInstruction* VisitCall(AstNode* stmt);
  FInstruction* VisitAssign(AstNode* stmt);

  FInstruction* VisitValue(AstNode* node);

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

  inline int instr_id();

  inline FLabel* loop_start();
  inline void loop_start(FLabel* loop_start);
  inline FLabel* loop_end();
  inline void loop_end(FLabel* loop_end);

  inline SourceMap* source_map();

 private:
  CodeSpace* space_;
  Root root_;

  int instr_id_;
  FLabel* loop_start_;
  FLabel* loop_end_;

  SourceMap* source_map_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_FULLGEN_H_
