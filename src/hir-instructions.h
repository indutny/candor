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

class HIRInstruction {
 public:
  enum Type {
    kNone,
    kGoto,
    kStoreLocal,
    kStoreContext,
    kStoreProperty,
    kLoadRoot,
    kLoadLocal,
    kLoadContext,
    kBranchBool,
    kAllocateObject
  };

  HIRInstruction(Type type) : type_(type),
                              block_(NULL),
                              result_(NULL),
                              prev_(NULL),
                              next_(NULL) {
  }

  virtual void Init(HIRBasicBlock* block);

  void Use(HIRValue* value);

  inline Type type() { return type_; }
  inline bool is(Type type) { return type_ == type; }

  inline List<HIRValue*, ZoneObject>* values() { return &values_; }

  inline void SetResult(HIRValue* result) {
    Use(result);
    values()->Push(result);
    result_ = result;
  }
  inline HIRValue* GetResult() { return result_; }

  inline HIRInstruction* prev() { return prev_; }
  inline void prev(HIRInstruction* prev) { prev_ = prev; }
  inline HIRInstruction* next() { return next_; }
  inline void next(HIRInstruction* next) { next_ = next; }

  virtual void Print(PrintBuffer* p);

 private:
  Type type_;
  HIRBasicBlock* block_;

  List<HIRValue*, ZoneObject> values_;

  HIRValue* result_;

  HIRInstruction* prev_;
  HIRInstruction* next_;
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
    Use(lhs);
    values()->Push(lhs);
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
    Use(clause);
    values()->Push(clause);
  }

  virtual void Init(HIRBasicBlock* block);

  inline HIRValue* clause() { return clause_; }
  inline HIRBasicBlock* left() { return left_; }
  inline HIRBasicBlock* right() { return right_; }

 private:
  HIRValue* clause_;
  HIRBasicBlock* left_;
  HIRBasicBlock* right_;
};

class HIRGoto : public HIRInstruction, public ZoneObject {
 public:
  HIRGoto() : HIRInstruction(kGoto) {
  }
};

class HIRLoadRoot : public HIRLoadBase {
 public:
  HIRLoadRoot(HIRValue* value)
      : HIRLoadBase(kLoadRoot, value) {
  }
};

class HIRLoadLocal : public HIRLoadBase {
 public:
  HIRLoadLocal(HIRValue* value)
      : HIRLoadBase(kLoadLocal, value) {
  }
};

class HIRLoadContext : public HIRLoadBase {
 public:
  HIRLoadContext(HIRValue* value)
      : HIRLoadBase(kLoadContext, value) {
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
};

class HIRAllocateObject : public HIRInstruction {
 public:
  enum ObjectKind {
    kObject,
    kArray
  };

  HIRAllocateObject(ObjectKind kind, int size)
      : HIRInstruction(kAllocateObject),
        kind_(kind),
        size_(PowerOfTwo(size << 1)) {
  }

  void Init(HIRBasicBlock* block);

  inline ObjectKind kind() { return kind_; }
  inline int size() { return size_; }

 private:
  ObjectKind kind_;
  int size_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_HIR_INSTRUCTIONS_H_
