#include "lir.h"
#include <stdlib.h> // NULL

namespace candor {
namespace internal {

LIR::LIR(Heap* heap, HIR* hir) : heap_(heap), hir_(hir) {
}


char* LIR::Generate() {
  return NULL;
}

} // namespace internal
} // namespace candor
