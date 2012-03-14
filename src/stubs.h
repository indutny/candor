#ifndef _SRC_STUBS_H_
#define _SRC_STUBS_H_

#include "macroassembler.h" // Masm
#include "code-space.h" // CodeSpace
#include "zone.h" // Zone
#include "ast.h" // BinOpType

#include <stdlib.h> // NULL
#include <stdint.h> // uint32_t

namespace candor {
namespace internal {

#define STUBS_LIST(V)\
    V(Entry)\
    V(Allocate)\
    V(CallBinding)\
    V(CollectGarbage)\
    V(Throw)\
    V(Typeof)\
    V(Sizeof)\
    V(Keysof)\
    V(LookupProperty)\
    V(CoerceToBoolean)\

#define BINARY_STUBS_LIST(V)\
    V(Add)\
    V(Sub)\
    V(Mul)\
    V(Div)\
    V(Mod)\
    V(BAnd)\
    V(BOr)\
    V(BXor)\
    V(Shl)\
    V(Shr)\
    V(UShl)\
    V(UShr)\
    V(Eq)\
    V(StrictEq)\
    V(Ne)\
    V(StrictNe)\
    V(Lt)\
    V(Gt)\
    V(Le)\
    V(Ge)\
    V(LOr)\
    V(LAnd)

class BaseStub {
 public:
  enum StubType {
#define STUB_ENUM(V) k##V,
    STUBS_LIST(STUB_ENUM)
#undef STUB_ENUM
#define BINARY_STUB_ENUM(V) kBinary##V,
    BINARY_STUBS_LIST(BINARY_STUB_ENUM)
#undef BINARY_STUB_ENUM
    kNone
  };

  BaseStub(CodeSpace* space, StubType type);

  void GeneratePrologue(int stack_slots);
  void GenerateEpilogue(int args);

  virtual void Generate() = 0;

  inline CodeSpace* space() { return space_; }
  inline Masm* masm() { return &masm_; }

  inline StubType type() { return type_; }
  inline bool is(StubType type) { return type_ == type; }

 protected:
  CodeSpace* space_;
  Masm masm_;
  StubType type_;
};

#define STUB_CLASS_DECL(V)\
    class V##Stub : public BaseStub {\
     public:\
      V##Stub(CodeSpace* space) : BaseStub(space, k##V) {}\
      void Generate();\
    };
STUBS_LIST(STUB_CLASS_DECL)
#undef STUB_CLASS_DECL

class BinaryOpStub : public BaseStub {
 public:
  // TODO: Use some type instead of kNone
  BinaryOpStub(CodeSpace* space, BinOp::BinOpType type) :
      BaseStub(space, kNone), type_(type) {
  }

  BinOp::BinOpType type() { return type_; }

  void Generate();

 protected:
  BinOp::BinOpType type_;
};

#define BINARY_STUB_CLASS_DECL(V)\
    class Binary##V##Stub : public BinaryOpStub {\
     public:\
      Binary##V##Stub(CodeSpace* space) : BinaryOpStub(space, BinOp::k##V) {}\
    };
BINARY_STUBS_LIST(BINARY_STUB_CLASS_DECL)
#undef BINARY_STUB_CLASS_DECL

#define STUB_LAZY_ALLOCATOR(V)\
    char* Get##V##Stub() {\
      if (stub_##V##_ == NULL) {\
        Zone zone;\
        V##Stub stub(space());\
        stub.Generate();\
        stub_##V##_ = space()->Put(stub.masm());\
      }\
      return stub_##V##_;\
    }

#define BINARY_STUB_LAZY_ALLOCATOR(V) STUB_LAZY_ALLOCATOR(Binary##V)

#define STUB_PROPERTY(V) char* stub_##V##_;
#define STUB_PROPERTY_INIT(V) stub_##V##_ = NULL;
#define BINARY_STUB_PROPERTY(V) char* stub_Binary##V##_;
#define BINARY_STUB_PROPERTY_INIT(V) stub_Binary##V##_ = NULL;

class Stubs {
 public:
  Stubs(CodeSpace* space) : space_(space) {
    STUBS_LIST(STUB_PROPERTY_INIT)
    BINARY_STUBS_LIST(BINARY_STUB_PROPERTY_INIT)
  }

  inline CodeSpace* space() { return space_; }

  STUBS_LIST(STUB_LAZY_ALLOCATOR)
  BINARY_STUBS_LIST(BINARY_STUB_LAZY_ALLOCATOR)
 protected:
  CodeSpace* space_;

  STUBS_LIST(STUB_PROPERTY)
  BINARY_STUBS_LIST(BINARY_STUB_PROPERTY)
};

#undef BINARY_STUB_LAZY_ALLOCATOR
#undef STUB_LAZY_ALLOCATOR
#undef BINARY_STUB_PROPERTY_INIT
#undef BINARY_STUB_PROPERTY
#undef STUB_PROPERTY_INIT
#undef STUB_PROPERTY


#undef STUBS_LIST
#undef BINARY_STUBS_LIST

} // namespace internal
} // namespace candor

#endif // _SRC_STUBS_H_
