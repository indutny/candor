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
class HIRBasicBlock;
class HIRLoopShuffle;
class LIR;
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

  enum MoveStatus {
    kToMove,
    kBeingMoved,
    kMoved
  };

  LIROperand(Type type, off_t value) : type_(type),
                                       value_(value),
                                       move_status_(kToMove) {
  }

  LIROperand(Type type, char* value) : type_(type),
                                       move_status_(kToMove) {
    value_ = reinterpret_cast<off_t>(value);
  }

  inline Type type() { return type_; }
  inline bool is_register() { return type_ == kRegister; }
  inline bool is_spill() { return type_ == kSpill; }
  inline bool is_immediate() { return type_ == kImmediate; }

  inline bool is_equal(LIROperand* op) {
    return !is_immediate() && type() == op->type() && value() == op->value();
  }

  // Whether operand is being moved by HIRParallelMove::Reorder, or not
  inline MoveStatus move_status() { return move_status_; }
  inline void move_status(MoveStatus move_status) {
    move_status_ = move_status;
  }

  inline off_t value() { return value_; }

 private:
  Type type_;
  off_t value_;
  MoveStatus move_status_;
};

// For sorted insertions
class HIRValueEndShape {
 public:
  static inline int Compare(HIRValue* l, HIRValue* r);
};

// Auto-releasing list
class LIRReleaseList : public ZoneList<LIROperand*> {
 public:
  LIRReleaseList(LIR* lir) : lir_(lir) {
  }

  ~LIRReleaseList();

  inline LIR* lir() { return lir_; }

 private:
  LIR* lir_;
};

// LIR class
class LIR {
 public:
  LIR(Heap* heap, HIR* hir);

  // Calculate values' liveness ranges
  void CalculateLiveness();

  // Prune phis that ain't used anywhere, extend inputs/phi liveness
  // ranges to include phi/inputs itself.
  void PrunePhis();

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

  // Short-hand for generating after(reverse) movement and reseting it
  // (used in BranchBool)
  void GenerateReverseMove(Masm* masm, HIRInstruction* hinstr);

  // Stores all values and their operand to shuffle list
  void StoreLoopInvariants(HIRBasicBlock* block,
                           ZoneList<HIRLoopShuffle*>* shuffle);

  // Put spill used in movements to active_values() list to
  // release it automatically after reverse instruction
  void InsertMoveSpill(HIRParallelMove* move,
                       HIRParallelMove* reverse,
                       LIROperand* spill);

  // Spill all active (in-use) registers that will be live after hinstr.
  void SpillActive(Masm* masm, HIRInstruction* hinstr);

  // Linear scan methods:

  // Find all values that occupy registers/spills, but not used anymore
  // Release them and put into freelist
  void ExpireOldValues(HIRInstruction* hinstr, ZoneList<HIRValue*>* list);

  // Allocate register for instruction (may spill some active values)
  LIROperand* AllocateRegister(HIRInstruction* hinstr);

  // Go through active values and move all uses of register into spill
  void SpillRegister(HIRInstruction* hinstr, LIROperand* reg);

  // Add phis assignments to movement
  void MovePhis(HIRInstruction* hinstr);

  // Checks if operand is used anywhere
  inline bool IsInUse(LIROperand* operand);

  // Release operand in registers() or spills()
  inline void Release(LIROperand* operand);

  // Wrapper over spills FreeList and incremental index
  inline LIROperand* GetSpill();

  // Convert HIR instruction into LIR instruction
  inline LIRInstruction* Cast(HIRInstruction* instr);

  // List of active values sorted by increasing live_range()->start
  inline ZoneList<HIRValue*>* active_values() { return &active_values_; }
  // List of active values that can be spilled
  inline ZoneList<HIRValue*>* spill_values() { return &spill_values_; }

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
  ZoneList<HIRValue*> spill_values_;

  FreeList<int, 128> registers_;
  FreeList<int, 128> spills_;
  int spill_count_;

  Heap* heap_;
  HIR* hir_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_H_
