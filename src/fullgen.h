#ifndef _SRC_FULLGEN_H_
#define _SRC_FULLGEN_H_

#include "visitor.h"
#include "ast.h" // AstNode, FunctionLiteral
#include "zone.h" // ZoneObject
#include "utils.h" // List

#if __ARCH == x64
#include "x64/macroassembler-x64.h"
#include "x64/macroassembler-x64-inl.h"
#else
#include "ia32/macroassembler-ia32.h"
#endif

#include <assert.h> // assert
#include <stdint.h> // uint32_t

namespace candor {
namespace internal {

// Forward declaration
class Heap;
class Register;

class FFunction : public ZoneObject {
 public:
  FFunction(Masm* masm) : masm_(masm), addr_(0) {
  }

  void Use(uint32_t offset);
  void Allocate(uint32_t addr);
  virtual void Generate() = 0;

  inline Masm* masm() { return masm_; }

 protected:
  Masm* masm_;
  List<RelocationInfo*, ZoneObject> uses_;
  uint32_t addr_;
};

class FAstRegister : public AstValue {
 public:
  FAstRegister(Register reg) : AstValue(kRegister), reg_(reg) {
  }

  static inline FAstRegister* Cast(AstValue* value) {
    assert(value->is_register());
    return reinterpret_cast<FAstRegister*>(value);
  }

  inline Register reg() { return reg_; }

 protected:
  Register reg_;
};

class FAstOperand : public AstValue {
 public:
  FAstOperand(Operand* op) : AstValue(kOperand), op_(op) {
  }

  static inline FAstOperand* Cast(AstValue* value) {
    assert(value->is_operand());
    return reinterpret_cast<FAstOperand*>(value);
  }

  inline Operand* op() { return op_; }

 protected:
  Operand* op_;
};

// Generates non-optimized code by visiting each node in AST tree in-order
class Fullgen : public Masm, public Visitor {
 public:
  class CandorFunction : public FFunction {
   public:
    CandorFunction(Fullgen* fullgen, FunctionLiteral* fn) : FFunction(fullgen),
                                                            fullgen_(fullgen),
                                                            fn_(fn) {
    }

    inline Fullgen* fullgen() { return fullgen_; }
    inline FunctionLiteral* fn() { return fn_; }

    static inline CandorFunction* Cast(void* value) {
      return reinterpret_cast<CandorFunction*>(value);
    }

    void Generate();

   protected:
    Fullgen* fullgen_;
    FunctionLiteral* fn_;
  };

  enum VisitorType {
    kValue,
    kSlot
  };

  Fullgen(Heap* heap);

  uint32_t Generate(AstNode* ast);

  void GeneratePrologue(AstNode* stmt);
  void GenerateEpilogue(AstNode* stmt);

  // Stores reference to HValue inside root context
  void PlaceInRoot(char* addr);

  // Alloctes HContext object for root variables
  char* AllocateRoot();

  AstNode* VisitFunction(AstNode* stmt);
  AstNode* VisitCall(AstNode* stmt);
  AstNode* VisitAssign(AstNode* stmt);

  AstNode* VisitValue(AstNode* node);

  AstNode* VisitNumber(AstNode* node);
  AstNode* VisitNil(AstNode* node);
  AstNode* VisitTrue(AstNode* node);
  AstNode* VisitFalse(AstNode* node);
  AstNode* VisitString(AstNode* node);
  AstNode* VisitProperty(AstNode* node);

  void ConvertToBoolean();

  AstNode* VisitIf(AstNode* node);
  AstNode* VisitWhile(AstNode* node);

  AstNode* VisitMember(AstNode* node);
  AstNode* VisitObjectLiteral(AstNode* node);
  AstNode* VisitArrayLiteral(AstNode* node);

  AstNode* VisitReturn(AstNode* node);

  AstNode* VisitUnOp(AstNode* node);
  AstNode* VisitBinOp(AstNode* node);

  AstNode* VisitForValue(AstNode* node, Register reg);
  AstNode* VisitForSlot(AstNode* node, Operand* op, Register base);

  inline Heap* heap() { return heap_; }
  inline bool visiting_for_value() { return visitor_type_ == kValue; }
  inline bool visiting_for_slot() { return visitor_type_ == kSlot; }
  inline List<FFunction*, ZoneObject>* fns() { return &fns_; }
  inline void current_function(CandorFunction* fn) { current_function_ = fn; }
  inline CandorFunction* current_function() { return current_function_; }
  inline List<char*, ZoneObject>* root_context() { return &root_context_; }

 private:
  Heap* heap_;
  VisitorType visitor_type_;
  List<FFunction*, ZoneObject> fns_;
  CandorFunction* current_function_;
  List<char*, ZoneObject> root_context_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_FULLGEN_H_
