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

class LIROperand : public ZoneObject {
 public:
  enum Type {
    kRegister,
    kSpill,
    kImmediate
  };

  LIROperand(Type type, off_t value) : type_(type), value_(value) {
  }

  LIROperand(Type type, char* value) : type_(type) {
    value_ = reinterpret_cast<off_t>(value);
  }

  inline Type type() { return type_; }
  inline bool is_register() { return type_ == kRegister; }
  inline bool is_spill() { return type_ == kSpill; }
  inline bool is_immediate() { return type_ == kImmediate; }

  inline off_t value() { return value_; }

 private:
  Type type_;
  off_t value_;
};

class LIR {
 public:
  struct RegisterFreeList {
    int list[128];
    int length;
  };

  LIR(Heap* heap, HIR* hir);

  void CalculateLiveness();

  void Generate(Masm* masm);

  inline LIRInstruction* Cast(HIRInstruction* instr);

  // Linear scan methods
  void ExpireOldValues(HIRInstruction* hinstr);
  void SpillAllocated(HIRParallelMove* move);
  void GenerateInstruction(Masm* masm, HIRInstruction* hinstr);

  // Insert into sorted list
  void AddToSpillCandidates(HIRValue* value);

  // List of active values sorted by increasing live_range()->start
  inline List<HIRValue*, ZoneObject>* active_values() {
    return &active_values_;
  }

  // List of active values sorted by decrasing live_range()->end
  inline List<HIRValue*, ZoneObject>* spill_candidates() {
    return &spill_candidates_;
  }

  inline FreeList<int, 128>* registers() { return &registers_; }
  inline FreeList<int, 128>* spills() { return &spills_; }

  inline int spills_count() { return spills_count_; }
  inline int get_new_spill() { return spills_count_++; }

  inline Heap* heap() { return heap_; }
  inline HIR* hir() { return hir_; }

 private:
  List<HIRValue*, ZoneObject> active_values_;
  List<HIRValue*, ZoneObject> spill_candidates_;

  FreeList<int, 128> registers_;
  FreeList<int, 128> spills_;
  int spills_count_;

  Heap* heap_;
  HIR* hir_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_H_
