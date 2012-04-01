#ifndef _SRC_HIR_INSTRUCTIONS_H_
#define _SRC_HIR_INSTRUCTIONS_H_

#include "utils.h" // PowerOfTwo
#include "zone.h" // ZoneObject

#include <stdlib.h> // NULL
#include <stdint.h> // int64_t
#include <assert.h> // assert

namespace candor {
namespace internal {

// Forward declarations
class HIRBasicBlock;
class HIRValue;
class HIRPhi;
class LIROperand;
class RelocationInfo;

// Instruction base class
class HIRInstruction {
 public:
  enum Type {
    kNone,
    kNop,

    // Instruction w/o side-effects
    kParallelMove,
    kEntry,
    kReturn,
    kGoto,
    kStoreLocal,
    kStoreContext,
    kStoreProperty,
    kLoadRoot,
    kLoadLocal,
    kLoadContext,

    // Branching
    kBranchBool,

    // Stubs and instructions with side-effects
    kCall,
    kAllocateContext,
    kAllocateFunction,
    kAllocateObject
  };

  HIRInstruction(Type type) : type_(type),
                              block_(NULL),
                              result_(NULL),
                              prev_(NULL),
                              next_(NULL) {
  }

  // Called by HIR to associate instruction with block and to
  // give instruction a monothonic id
  virtual void Init(HIRBasicBlock* block, int id);

  // Wrapper to push into value->uses()
  void Use(HIRValue* value);

  // Type checks
  inline Type type() { return type_; }
  inline bool is(Type type) { return type_ == type; }

  // Block in which this instruction is located
  inline HIRBasicBlock* block() { return block_; }

  // Instruction with side
  virtual bool HasSideEffects() const { return false; };

  // Instruction takes multiple inputs
  inline void SetInput(HIRValue* input) {
    Use(input);
    values()->Push(input);
  }

  // And one output
  inline void SetResult(HIRValue* result) {
    assert(result_ == NULL);

    Use(result);
    values()->Push(result);
    result_ = result;
  }
  inline HIRValue* GetResult() { return result_; }

  // List of all used values
  inline ZoneList<HIRValue*>* values() { return &values_; }

  // Links to previous and next instructions
  inline HIRInstruction* prev() { return prev_; }
  inline void prev(HIRInstruction* prev) { prev_ = prev; }
  inline HIRInstruction* next() { return next_; }
  inline void next(HIRInstruction* next) { next_ = next; }

  // For liveness-range calculations
  int id() { return id_; }

  // Debug printing
  virtual void Print(PrintBuffer* p);

 private:
  Type type_;
  HIRBasicBlock* block_;

  ZoneList<HIRValue*> values_;

  HIRValue* result_;

  HIRInstruction* prev_;
  HIRInstruction* next_;

  int id_;
};

class HIRLoadBase : public HIRInstruction, public ZoneObject {
 public:
  HIRLoadBase(Type type, HIRValue* value) : HIRInstruction(type),
                                            value_(value) {
    SetResult(value);
  }

  inline HIRValue* value() { return value_; }

 private:
  HIRValue* value_;
};

class HIRStoreBase : public HIRInstruction, public ZoneObject {
 public:
  HIRStoreBase(Type type, HIRValue* lhs, HIRValue* rhs) : HIRInstruction(type),
                                                          lhs_(lhs),
                                                          rhs_(rhs) {
    SetInput(lhs);
    SetResult(rhs);
  }

  inline HIRValue* lhs() { return lhs_; }
  inline HIRValue* rhs() { return rhs_; }

 private:
  HIRValue* lhs_;
  HIRValue* rhs_;
};

class HIRBranchBase : public HIRInstruction, public ZoneObject {
 public:
  HIRBranchBase(Type type,
                HIRValue* clause,
                HIRBasicBlock* left,
                HIRBasicBlock* right) : HIRInstruction(type),
                                        clause_(clause),
                                        left_(left),
                                        right_(right) {
    SetInput(clause);
  }

  virtual void Init(HIRBasicBlock* block, int id);

  inline HIRValue* clause() { return clause_; }
  inline HIRBasicBlock* left() { return left_; }
  inline HIRBasicBlock* right() { return right_; }

 private:
  HIRValue* clause_;
  HIRBasicBlock* left_;
  HIRBasicBlock* right_;
};

class HIRStubCall : public HIRInstruction, public ZoneObject {
 public:
  HIRStubCall(Type type) : HIRInstruction(type) {
  }

  void Init(HIRBasicBlock* block, int id);
  virtual bool HasSideEffects() const { return true; };
};

class HIRParallelMove : public HIRInstruction {
 public:
  typedef ZoneList<LIROperand*> OperandList;

  enum InsertionType {
    kBefore,
    kAfter
  };

  HIRParallelMove(HIRInstruction* instr, InsertionType type);

  void AddMove(LIROperand* source, LIROperand* target);

  // Order movements to prevent "overlapping"
  void Reorder();

  static inline HIRParallelMove* Cast(HIRInstruction* instr) {
    assert(instr->type() == kParallelMove);
    return reinterpret_cast<HIRParallelMove*>(instr);
  }

  inline OperandList* sources() { return &sources_; }
  inline OperandList* targets() { return &targets_; }
  inline OperandList* raw_sources() { return &raw_sources_; }
  inline OperandList* raw_targets() { return &raw_targets_; }

 private:
  // For recursive reordering
  void Reorder(LIROperand* source, LIROperand* target);

  // Sources/Targets after reordering
  OperandList sources_;
  OperandList targets_;

  // Sources/Targets before reordering (because it's cheaper to leave it here)
  OperandList raw_sources_;
  OperandList raw_targets_;
};

class HIRNop : public HIRInstruction {
 public:
  HIRNop() : HIRInstruction(kNop) {
  }
};

class HIREntry : public HIRInstruction {
 public:
  HIREntry() : HIRInstruction(kEntry) {
  }
};

class HIRReturn : public HIRInstruction {
 public:
  HIRReturn(HIRValue* value) : HIRInstruction(kReturn) {
    SetInput(value);
  }
};

class HIRGoto : public HIRInstruction {
 public:
  HIRGoto() : HIRInstruction(kGoto) {
  }
};

class HIRLoadRoot : public HIRLoadBase {
 public:
  HIRLoadRoot(HIRValue* value) : HIRLoadBase(kLoadRoot, value), value_(value) {
  }

  inline HIRValue* value() { return value_; }

 private:
  HIRValue* value_;
};

class HIRLoadLocal : public HIRLoadBase {
 public:
  HIRLoadLocal(HIRValue* value) : HIRLoadBase(kLoadLocal, value) {
  }
};

class HIRLoadContext : public HIRLoadBase {
 public:
  HIRLoadContext(HIRValue* value) : HIRLoadBase(kLoadContext, value) {
  }
};

class HIRStoreLocal : public HIRStoreBase {
 public:
  HIRStoreLocal(HIRValue* lhs, HIRValue* rhs)
      : HIRStoreBase(kStoreLocal, lhs, rhs) {
  }
};

class HIRStoreContext : public HIRStoreBase {
 public:
  HIRStoreContext(HIRValue* lhs, HIRValue* rhs)
      : HIRStoreBase(kStoreContext, lhs, rhs) {
  }
};

class HIRStoreProperty : public HIRStoreBase {
 public:
  HIRStoreProperty(HIRValue* lhs, HIRValue* rhs)
      : HIRStoreBase(kStoreProperty, lhs, rhs) {
  }
};

class HIRBranchBool : public HIRBranchBase {
 public:
  HIRBranchBool(HIRValue* clause, HIRBasicBlock* left, HIRBasicBlock* right)
      : HIRBranchBase(kBranchBool, clause, left, right) {
  }

  virtual bool HasSideEffects() const { return true; };
};

class HIRAllocateContext : public HIRStubCall {
 public:
  HIRAllocateContext(int size) : HIRStubCall(kAllocateContext),
                                 size_(size) {
  }

  inline int size() { return size_; }

 private:
  int size_;
};

class HIRCall : public HIRStubCall {
 public:
  HIRCall(HIRValue* fn) : HIRStubCall(kCall), fn_(fn) {
  }

  void AddArg(HIRValue* arg);

  inline HIRValue* fn() { return fn_; }
  inline ZoneList<HIRValue*>* args() { return &args_; }

 private:
  HIRValue* fn_;
  ZoneList<HIRValue*> args_;
};

class HIRAllocateFunction : public HIRStubCall {
 public:
  HIRAllocateFunction(HIRBasicBlock* body, int argc)
      : HIRStubCall(kAllocateFunction),
        argc_(argc),
        body_(body) {
  }

  inline int argc() { return argc_; }
  inline HIRBasicBlock* body() { return body_; }

 private:
  int argc_;
  HIRBasicBlock* body_;
};

class HIRAllocateObject : public HIRStubCall {
 public:
  enum ObjectKind {
    kObject,
    kArray
  };

  HIRAllocateObject(ObjectKind kind, int size)
      : HIRStubCall(kAllocateObject),
        kind_(kind),
        size_(PowerOfTwo(size << 1)) {
  }

  inline ObjectKind kind() { return kind_; }
  inline int size() { return size_; }

 private:
  ObjectKind kind_;
  int size_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_HIR_INSTRUCTIONS_H_
