#ifndef _SRC_LIR_H_
#define _SRC_LIR_H_

#include "lir-allocator.h"
#include "lir-allocator-inl.h"
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
class LIR;
class LIRInstruction;
struct Register;

// For sorted insertions
class HIRValueEndShape {
 public:
  static inline int Compare(HIRValue* l, HIRValue* r);
};

// Auto-releasing list,
// puts all operands in spill_values()
class LIRSpillList : public ZoneList<HIRValue*> {
 public:
  LIRSpillList(LIR* lir) : lir_(lir) {
  }

  ~LIRSpillList();

  inline LIR* lir() { return lir_; }

 private:
  LIR* lir_;
};

// LIR class
class LIR {
 public:
  LIR(Heap* heap, HIR* hir, Masm* masm);

  // Calculate values' liveness ranges
  void CalculateLiveness();

  // Prune phis that ain't used anywhere, extend inputs/phi liveness
  // ranges to include phi/inputs itself.
  void PrunePhis();

  // Generate linked-list of instruction for `hir`
  void Translate();

  // Generate machine code for linked-list of instructions
  void Generate();

  // Translate specific instruction into LIR representation
  // (including register allocation, spilling/unspilling)
  //
  // First of all, instruction's values will be placed in registers/spills.
  // Then scratch registers will be created, and only after it - the result
  // register.
  //
  // After instruction, scratch registers will be put back into FreeList
  //
  void TranslateInstruction(HIRInstruction* hinstr);

  // Put spill used in movements to active_values() list to
  // release it automatically after reverse instruction
  void InsertMoveSpill(HIRParallelMove* move,
                       HIRParallelMove* reverse,
                       LIROperand* spill);

  // Spill all active (in-use) registers that will be live after hinstr.
  void SpillActive(HIRInstruction* hinstr);

  // Linear scan methods:

  // Find all values that occupy registers/spills, but not used anymore
  // Release them and put into freelist
  void ExpireOldValues(HIRInstruction* hinstr, ZoneList<HIRValue*>* list);

  // Allocate register for instruction (may spill some active values)
  LIROperand* GetRegister(HIRInstruction* hinstr);
  LIROperand* GetRegister(HIRInstruction* hinstr, HIRValue* value);
  LIROperand* GetRegister(HIRInstruction* hinstr,
                          HIRValue* value,
                          LIROperand* reg);

  // Wrapper over spills FreeList and incremental index
  LIROperand* GetSpill();
  LIROperand* GetSpill(HIRInstruction* hinstr, HIRValue* value);

  // Go through active values and move all uses of register into spill
  void SpillRegister(HIRInstruction* hinstr, LIROperand* reg);

  // Debug printing
  void Print(char* buffer, uint32_t size);

  // Changes value's operand and creates move if needed
  inline void ChangeOperand(HIRInstruction* hinstr,
                            HIRValue* value,
                            LIROperand* operand);

  // Checks if operand is used anywhere
  inline bool IsInUse(LIROperand* operand);

  // Release operand in registers() or spills()
  inline void Release(LIROperand* operand);

  // Convert HIR instruction into LIR instruction
  inline LIRInstruction* Cast(HIRInstruction* instr);

  // Adds instruction to linked list
  inline void AddInstruction(LIRInstruction* instr);

  // List of active values sorted by increasing live_range()->start
  inline ZoneList<HIRValue*>* active_values() { return &active_values_; }
  // List of active values that can be spilled
  inline ZoneList<HIRValue*>* spill_values() { return &spill_values_; }

  // List of spills that should be commited to spill_values() only after
  // instruction will be generated
  inline LIRSpillList* spill_list() { return spill_list_; }
  inline void spill_list(LIRSpillList* spill_list) { spill_list_ = spill_list; }

  inline FreeList<int, 128>* registers() { return &registers_; }
  inline FreeList<int, 128>* spills() { return &spills_; }

  // NOTE: spill with id = -1 is reserved as ParallelMove's scratch
  inline int spill_count() { return spill_count_; }
  inline void spill_count(int spill_count) { spill_count_ = spill_count; }
  inline int get_new_spill() { return spill_count_++; }

  // Temporary spill for movements
  inline LIROperand* tmp_spill() { return tmp_spill_; }
  inline void tmp_spill(LIROperand* tmp_spill) { tmp_spill_ = tmp_spill; }

  inline Heap* heap() { return heap_; }
  inline HIR* hir() { return hir_; }
  inline Masm* masm() { return masm_; }

 private:
  ZoneList<HIRValue*> active_values_;
  ZoneList<HIRValue*> spill_values_;
  LIRSpillList* spill_list_;

  FreeList<int, 128> registers_;
  FreeList<int, 128> spills_;
  int spill_count_;

  LIROperand* tmp_spill_;
  LIRInstruction* first_instruction_;
  LIRInstruction* last_instruction_;

  Heap* heap_;
  HIR* hir_;
  Masm* masm_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_H_
