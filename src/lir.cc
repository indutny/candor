#include "hir.h"
#include "lir.h"
#include "lir-inl.h"

namespace candor {
namespace internal {
namespace lir {

LGen::LGen(hir::HGen* hir) : block_id_(0), instr_id_(0) {
}


LOperand::LOperand(int index, Type type) : index_(index), type_(type) {
}


LUse* LOperand::Use(LUse::Type type, LInstruction* instr) {
  LUse* use = new LUse(this, type, instr);

  uses_.InsertSorted<LUseShape>(use);

  return use;
}


LRange* LOperand::AddRange(int start, int end) {
  LRange* range = new LRange(this, start, end);

  ranges_.InsertSorted<LRangeShape>(range);

  return range;
}


LUnallocated::LUnallocated(int index) : LOperand(index, kUnallocated) {
}


LRegister::LRegister(int index) : LOperand(index, kRegister) {
}


LStackSlot::LStackSlot(int index) : LOperand(index, kStackSlot) {
}


LRange::LRange(LOperand* op, int start, int end) : op_(op),
                                                   start_(start),
                                                   end_(end) {
}


int LRangeShape::Compare(LRange* a, LRange* b) {
  return a->start() > b->start() ? 1 : a->start() < b->start() ? -1 : 0;
}


LUse::LUse(LOperand* op, Type type, LInstruction* instr) : op_(op),
                                                           type_(type),
                                                           instr_(instr) {
}


int LUseShape::Compare(LUse* a, LUse* b) {
  return a->instr()->id > b->instr()->id ? 1 :
         a->instr()->id < b->instr()->id ? -1 : 0;
}

} // namespace lir
} // namespace internal
} // namespace candor
