#include "pic.h"
#include "code-space.h" // CodeSpace
#include "macroassembler.h" // Masm

namespace candor {
namespace internal {

char* PIC::Generate() {
  Masm masm(space_);

  return space_->Put(&masm);
}

} // namespace internal
} // namespace candor
