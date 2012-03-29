#include "hir-instructions.h"
#include "hir.h"

namespace candor {
namespace internal {

void HIRInstruction::Init(HIRBasicBlock* block) {
  block_ = block;
}

} // namespace internal
} // namespace candor
