#include "fullgen.h"
#include "fullgen-instructions.h"
#include "fullgen-instructions-inl.h"

namespace candor {
namespace internal {

FInstruction::FInstruction(Type type) : id(-1), type_(type), input_count_(0) {
  result = NULL;
  inputs[0] = NULL;
  inputs[1] = NULL;
  inputs[2] = NULL;
}


void FInstruction::Print(PrintBuffer* p) {
  p->Print("%d ", id);
  if (result != NULL) result->Print(p);
  p->Print("= %s(", id, TypeToStr(type_));
  for (int i = 0; i < input_count_; i++) {
    inputs[i]->Print(p);
    if (i + 1 < input_count_) p->Print(", ");
  }
  p->Print(")\n");
}

} // internal
} // candor
