#ifndef _SRC_HIR_INSTRUCTIONS_H_
#define _SRC_HIR_INSTRUCTIONS_H_

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
    kStoreLocal,
    kStoreContext,
    kStoreProperty,
    kLoadRoot,
    kLoadLocal,
    kLoadContext
  };

  HIRInstruction(Type type) : type_(type), block_(NULL), result_(NULL) {
  }

  void Init(HIRBasicBlock* block);

  inline Type type() { return type_; }
  inline bool is(Type type) { return type_ == type; }

  inline void SetResult(HIRValue* result) { result_ = result; }
  inline HIRValue* GetResult() { return result_; }

 private:
  Type type_;
  HIRBasicBlock* block_;
  HIRValue* result_;
};

class HIRLoadBase : public HIRInstruction {
 public:
  HIRLoadBase(Type type, HIRValue* value) : HIRInstruction(type),
                                            value_(value) {
    SetResult(value);
  }

  inline HIRValue* value() { return value_; }

 private:
  HIRValue* value_;
};

class HIRStoreBase : public HIRInstruction {
 public:
  HIRStoreBase(Type type, HIRValue* lhs, HIRValue* rhs) : HIRInstruction(type),
                                                          lhs_(lhs),
                                                          rhs_(rhs) {
    SetResult(rhs);
  }

  inline HIRValue* lhs() { return lhs_; }
  inline HIRValue* rhs() { return rhs_; }

 private:
  HIRValue* lhs_;
  HIRValue* rhs_;
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

} // namespace internal
} // namespace candor

#endif // _SRC_HIR_INSTRUCTIONS_H_
