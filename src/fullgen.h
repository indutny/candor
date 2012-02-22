#ifndef _SRC_FULLGEN_H_
#define _SRC_FULLGEN_H_

#include "visitor.h"
#include "ast.h" // AstNode, FunctionLiteral
#include "zone.h" // ZoneObject
#include "utils.h" // List

#if __ARCH == x64
#include "x64/macroassembler-x64.h"
#else
#include "ia32/macroassembler-ia32.h"
#endif

namespace dotlang {

// Forward declaration
class Heap;
class Register;

// Generates non-optimized code by visiting each node in AST tree in-order
class Fullgen : public Masm, public Visitor {
 public:
  class FFunction : public ZoneObject {
   public:
    FFunction(Assembler* a, FunctionLiteral* fn) : asm_(a), fn_(fn) {
    }

    void Use(uint32_t offset);
    void Allocate(uint32_t address);

    inline FunctionLiteral* fn() { return fn_; }

   protected:
    Assembler* asm_;
    FunctionLiteral* fn_;
    List<RelocationInfo*, ZoneObject> uses_;
  };

  enum VisitorType {
    kValue,
    kSlot
  };

  Fullgen(Heap* heap) : Masm(heap),
                        Visitor(kPreorder),
                        heap_(heap),
                        visitor_type_(kSlot) {
  }

  void Generate(AstNode* ast);

  void GeneratePrologue(AstNode* stmt);
  void GenerateEpilogue();
  void GenerateLookup(AstNode* name);

  AstNode* VisitFunction(AstNode* stmt);
  AstNode* VisitAssign(AstNode* stmt);
  AstNode* VisitValue(AstNode* node);
  AstNode* VisitNumber(AstNode* node);
  AstNode* VisitReturn(AstNode* node);

  AstNode* VisitForValue(AstNode* node, Register reg);
  AstNode* VisitForSlot(AstNode* node, Operand* op, Register base);

  inline Heap* heap() { return heap_; }
  inline bool visiting_for_value() { return visitor_type_ == kValue; }
  inline bool visiting_for_slot() { return visitor_type_ == kSlot; }

 private:
  Heap* heap_;
  VisitorType visitor_type_;
  List<FFunction*, ZoneObject> fns_;
};

} // namespace dotlang

#endif // _SRC_FULLGEN_H_
