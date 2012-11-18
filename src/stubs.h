/**
 * Copyright (c) 2012, Fedor Indutny.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _SRC_STUBS_H_
#define _SRC_STUBS_H_

#include <stdlib.h>  // NULL
#include <stdint.h>  // uint32_t

#include "macroassembler.h"  // Masm
#include "macroassembler-inl.h"
#include "code-space.h"  // CodeSpace
#include "zone.h"  // Zone
#include "ast.h"  // BinOpType

namespace candor {
namespace internal {

#define STUBS_LIST(V)\
    V(Entry)\
    V(Allocate)\
    V(AllocateObject)\
    V(AllocateFunction)\
    V(CallBinding)\
    V(CollectGarbage)\
    V(Throw)\
    V(Typeof)\
    V(Sizeof)\
    V(Keysof)\
    V(LookupProperty)\
    V(PICMiss)\
    V(CoerceToBoolean)\
    V(CloneObject)\
    V(DeleteProperty)\
    V(HashValue)\
    V(StackTrace)\
    V(LoadVarArg)\
    V(StoreVarArg)

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
  virtual ~BaseStub() {
  }

  void GeneratePrologue();
  void GenerateEpilogue(int args = 0);

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

class BinOpStub : public BaseStub {
 public:
  // TODO(indutny): Use some type instead of kNone
  BinOpStub(CodeSpace* space, BinOp::BinOpType type) :
      BaseStub(space, kNone), type_(type) {
  }

  BinOp::BinOpType type() { return type_; }

  void Generate();

 protected:
  BinOp::BinOpType type_;
};

#define BINARY_STUB_CLASS_DECL(V)\
    class Binary##V##Stub : public BinOpStub {\
     public:\
      Binary##V##Stub(CodeSpace* space) : BinOpStub(space, BinOp::k##V) {}\
    };
BINARY_STUBS_LIST(BINARY_STUB_CLASS_DECL)
#undef BINARY_STUB_CLASS_DECL

#define STUB_LAZY_ALLOCATOR(V)\
    char* Get##V##Stub() {\
      if (stub_##V##_ == NULL) {\
        Zone zone;\
        V##Stub stub(space());\
        stub.Generate();\
        CodeChunk* chunk = space()->CreateChunk("__" #V "__stub__", "", 0); \
        space()->Put(chunk, stub.masm()); \
        stub_##V##_ = chunk->addr(); \
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
  explicit Stubs(CodeSpace* space) : space_(space) {
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

}  // namespace internal
}  // namespace candor

#endif  // _SRC_STUBS_H_
