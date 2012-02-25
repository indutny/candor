#ifndef _SRC_STUBS_H_
#define _SRC_STUBS_H_

#include "fullgen.h" // FFunction
#include "zone.h" // ZoneObject

#include <stdlib.h> // NULL
#include <stdint.h> // uint32_t

namespace dotlang {

// Forward declarations
class Masm;
class RelocationInfo;

#define STUBS_LIST(V)\
    V(Allocate)\
    V(CoerceType)

class BaseStub : public FFunction {
 public:
  enum StubType {
#define STUB_ENUM(V) k##V,
    STUBS_LIST(STUB_ENUM)
#undef STUB_ENUM
    kNone
  };

  BaseStub(Masm* masm, StubType type);

  void GeneratePrologue();
  void GenerateEpilogue(int args);

  virtual void Generate() = 0;

  inline StubType type() { return type_; }
  inline bool is(StubType type) { return type_ == type; }

 protected:
  StubType type_;
};

class AllocateStub : public BaseStub {
 public:
  AllocateStub(Masm* masm) : BaseStub(masm, kAllocate) {
  }

  void Generate();
};

class CoerceTypeStub : public BaseStub {
 public:
  CoerceTypeStub(Masm* masm) : BaseStub(masm, kCoerceType) {
  }

  void Generate();
};

#define STUB_LAZY_ALLOCATOR(V)\
    V##Stub* Get##V##Stub() {\
      if (stub_##V##_ == NULL) {\
        stub_##V##_ = new V##Stub(masm_);\
        fullgen()->fns()->Push(stub_##V##_);\
      }\
      return stub_##V##_;\
    }

#define STUB_PROPERTY(V) V##Stub* stub_##V##_;
#define STUB_PROPERTY_INIT(V) stub_##V##_ = NULL;

class Stubs : public ZoneObject {
 public:
  Stubs(Masm* masm) : masm_(masm) {
    STUBS_LIST(STUB_PROPERTY_INIT)
  }

  inline Fullgen* fullgen() { return fullgen_; }
  inline void fullgen(Fullgen* fullgen) { fullgen_ = fullgen; }

  STUBS_LIST(STUB_LAZY_ALLOCATOR)
 protected:
  Masm* masm_;
  Fullgen* fullgen_;

  STUBS_LIST(STUB_PROPERTY)
};
#undef STUB_LAZY_ALLOCATOR
#undef STUB_PROPERTY


#undef STUBS_LIST

} // namespace dotlang

#endif // _SRC_STUBS_H_
