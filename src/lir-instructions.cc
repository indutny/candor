#include "lir.h" // LGen
#include "lir-inl.h" // LGen
#include "lir-instructions.h"
#include "lir-instructions-inl.h"

namespace candor {
namespace internal {

LInstruction::LInstruction(LGen* g) : g_(g), id(g->instr_id()) {
}

} // namespace internal
} // namespace candor
