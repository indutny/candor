#include "lir.h"
#include "lir-inl.h"

namespace candor {
namespace internal {
namespace lir {

LOperand::LOperand(Type type) : type_(type) {
}


LUnallocated::LUnallocated() : LOperand(kUnallocated) {
}


LRegister::LRegister() : LOperand(kRegister) {
}


LStackSlot::LStackSlot() : LOperand(kStackSlot) {
}

} // namespace lir
} // namespace internal
} // namespace candor
