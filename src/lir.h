#ifndef _SRC_LIR_H_
#define _SRC_LIR_H_

#include "zone.h"

#include <sys/types.h> // off_t

namespace candor {
namespace internal {

// Forward declarations
class Heap;
class Masm;
class HIR;
class HIRValue;
class HIRInstruction;
class HIRParallelMove;
class LIRInstruction;
struct Register;

// Operand for LIRInstruction
class LIROperand : public ZoneObject {
 public:
  enum Type {
    kRegister,
    kSpill,
    kImmediate
  };

  LIROperand(Type type, off_t value) : type_(type),
                                       value_(value),
                                       being_moved_(false) {
  }

  LIROperand(Type type, char* value) : type_(type),
                                       being_moved_(false) {
    value_ = reinterpret_cast<off_t>(value);
  }

  inline Type type() { return type_; }
  inline bool is_register() { return type_ == kRegister; }
  inline bool is_spill() { return type_ == kSpill; }
  inline bool is_immediate() { return type_ == kImmediate; }

  inline bool is_equal(LIROperand* op) {
    return type() == op->type() && value() == op->value();
  }

  // Whether operand is being moved by HIRParallelMove::Reorder, or not
  inline bool being_moved() { return being_moved_; }
  inline void being_moved(bool being_moved) { being_moved_ = being_moved; }

  inline off_t value() { return value_; }

 private:
  Type type_;
  off_t value_;
  bool being_moved_;
};

// LIR class
class LIR {
 public:
  LIR(Heap* heap, HIR* hir);

  // Calculate values' liveness ranges
  void CalculateLiveness();

  // Generate machine code for `hir`
  void Generate(Masm* masm);

  // Generate machine code for a specific instruction
  //
  // First of all, instruction's values will be placed in registers/spills.
  // Then scratch registers will be created, and only after it - the result
  // register.
  //
  // After instruction, scratch registers will be put back into FreeList
  //
  void GenerateInstruction(Masm* masm, HIRInstruction* hinstr);

  // Spill all active (in-use) registers that will be live after hinstr.
  // Returns reverse movement for restoring original registers values
  void SpillActive(Masm* masm, HIRInstruction* hinstr, HIRParallelMove* &move);

  // Linear scan methods:

  // Find all values that occupy registers/spills, but not used anymore
  // Release them and put into freelist
  void ExpireOldValues(HIRInstruction* hinstr);

  // Spill some value (that is in register) and release it's register
  void SpillAllocated(HIRParallelMove* move);

  // Allocate register for instruction
  int AllocateRegister(HIRInstruction* hinstr, HIRParallelMove* &move);

  // Insert value into sorted spills list
  void AddToSpillCandidates(HIRValue* value);

  // Wrapper over spills FreeList and incremental index
  inline LIROperand* GetSpill();

  // Convert HIR instruction into LIR instruction
  inline LIRInstruction* Cast(HIRInstruction* instr);

  // List of active values sorted by increasing live_range()->start
  inline ZoneList<HIRValue*>* active_values() {
    return &active_values_;
  }

  // List of active values sorted by decrasing live_range()->end
  inline ZoneList<HIRValue*>* spill_candidates() {
    return &spill_candidates_;
  }

  inline FreeList<int, 128>* registers() { return &registers_; }
  inline FreeList<int, 128>* spills() { return &spills_; }

  // NOTE: spill with id = -1 is reserved as ParallelMove's scratch
  inline int spill_count() { return spill_count_; }
  inline void spill_count(int spill_count) { spill_count_ = spill_count; }
  inline int get_new_spill() { return spill_count_++; }

  inline Heap* heap() { return heap_; }
  inline HIR* hir() { return hir_; }

 private:
  ZoneList<HIRValue*> active_values_;
  ZoneList<HIRValue*> spill_candidates_;

  FreeList<int, 128> registers_;
  FreeList<int, 128> spills_;
  int spill_count_;

  Heap* heap_;
  HIR* hir_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_H_
