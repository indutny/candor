#ifndef _SRC_FULLGEN_INSTRUCTIONS_H
#define _SRC_FULLGEN_INSTRUCTIONS_H

#include "zone.h"
#include "utils.h"

namespace candor {
namespace internal {

// Forward declarations
class Masm;

#define FULLGEN_INSTRUCTION_TYPES(V) \
    V(Nop)

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
  virtual void Generate(Masm* masm) = 0;

  int id;
  FOperand* result;
  FOperand* inputs[3];

 protected:
  Type type_;
  int input_count_;
};

#undef FULLGEN_INSTRUCTION_ENUM

} // internal
} // candor

#endif // _SRC_FULLGEN_INSTRUCTIONS_H
