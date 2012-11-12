#ifndef _SRC_FULLGEN_INSTRUCTIONS_H
#define _SRC_FULLGEN_INSTRUCTIONS_H

#include "macroassembler.h"
#include "zone.h"
#include "utils.h"

namespace candor {
namespace internal {

// Forward declarations
class Fullgen;

#define FULLGEN_INSTRUCTION_TYPES(V) \
    V(Nop) \
    V(Label) \
    V(Entry) \
    V(Return)

#define FULLGEN_INSTRUCTION_ENUM(V) \
    k##V,

class FInstruction : public ZoneObject {
 public:
  enum Type {
    FULLGEN_INSTRUCTION_TYPES(FULLGEN_INSTRUCTION_ENUM)
    kNone
  };

  FInstruction(Type type);

  static inline const char* TypeToStr(Type type);
  inline void SetResult(FOperand* op);
  inline void AddArg(FOperand* op);

  virtual void Print(PrintBuffer* p);
  virtual void Init(Fullgen* f);
  virtual void Generate(Masm* masm) = 0;

  int id;
  FOperand* result;
  FOperand* inputs[3];

 protected:
  Type type_;
  int input_count_;
};

#undef FULLGEN_INSTRUCTION_ENUM

#define FULLGEN_DEFAULT_METHODS(V) \
    void Generate(Masm* masm);

class FNop : public FInstruction {
 public:
  FNop() : FInstruction(kNop) {
  }

  FULLGEN_DEFAULT_METHODS(Nop)
};

class FLabel : public FInstruction {
 public:
  FLabel() : FInstruction(kLabel) {
  }

  FULLGEN_DEFAULT_METHODS(Label)

  Label label;
};

class FReturn : public FInstruction {
 public:
  FReturn() : FInstruction(kReturn) {
  }

  FULLGEN_DEFAULT_METHODS(Return)
};

#undef FULLGEN_DEFAULT_METHODS

} // internal
} // candor

#endif // _SRC_FULLGEN_INSTRUCTIONS_H
