#ifndef _SRC_FULLGEN_INSTRUCTION_INL_H_
#define _SRC_FULLGEN_INSTRUCTION_INL_H_

#include "fullgen-instructions.h"
#include <assert.h> // assert

namespace candor {
namespace internal {

inline void FInstruction::SetResult(FOperand* op) {
  assert(result == NULL);
  result = op;
}


inline void FInstruction::AddArg(FOperand* op) {
  assert(input_count_ < 3);
  inputs[input_count_++];
}

#define FULLGEN_INSTRUCTION_TYPE_TO_STR(V) \
    case k##V: return #V;

inline const char* FInstruction::TypeToStr(Type type) {
  switch (type) {
    FULLGEN_INSTRUCTION_TYPES(FULLGEN_INSTRUCTION_TYPE_TO_STR)
    default: UNEXPECTED break;
  }
  return "none";
}

#undef FULLGEN_INSTRUCTION_TYPE_TO_STR

} // namespace internal
} // namespace candor

#endif // _SRC_FULLGEN_INSTRUCTION_INL_H_
