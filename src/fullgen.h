#ifndef _SRC_FULLGEN_H_
#define _SRC_FULLGEN_H_

#include "visitor.h"
#include "code-space.h"
#include "ast.h" // AstNode, FunctionLiteral
#include "zone.h" // ZoneObject
#include "utils.h" // List

#include "macroassembler.h"

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

class FAstSpill : public AstValue {
 public:
  FAstSpill(Masm::Spill* spill) : AstValue(kSpill), spill_(spill) {
  }

  static inline FAstSpill* Cast(AstValue* value) {
    assert(value->is_spill());
    return reinterpret_cast<FAstSpill*>(value);
  }

  inline Masm::Spill* spill() { return spill_; }

 protected:
  Masm::Spill* spill_;
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

  Fullgen(CodeSpace* space);

  void Throw(Heap::Error err);

  void Generate(AstNode* ast);

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

  AstNode* VisitTypeof(AstNode* node);
  AstNode* VisitSizeof(AstNode* node);
  AstNode* VisitKeysof(AstNode* node);

  AstNode* VisitUnOp(AstNode* node);
  AstNode* VisitBinOp(AstNode* node);

  AstNode* VisitForValue(AstNode* node);
  AstNode* VisitForSlot(AstNode* node);

  inline void SetError(const char* message) {
    if (error_msg_ != NULL) return;

    error_msg_ = message;
  }

  inline bool has_error() { return error_msg_ != NULL; }
  inline const char* error_msg() { return error_msg_; }

  inline Heap* heap() { return space_->heap(); }
  inline bool visiting_for_value() { return visitor_type_ == kValue; }
  inline bool visiting_for_slot() { return visitor_type_ == kSlot; }
  inline List<FFunction*, ZoneObject>* fns() { return &fns_; }
  inline void current_function(CandorFunction* fn) { current_function_ = fn; }
  inline CandorFunction* current_function() { return current_function_; }
  inline List<char*, ZoneObject>* root_context() { return &root_context_; }

 private:
  CodeSpace* space_;
  VisitorType visitor_type_;
  List<FFunction*, ZoneObject> fns_;
  CandorFunction* current_function_;
  List<char*, ZoneObject> root_context_;

  const char* error_msg_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_FULLGEN_H_
