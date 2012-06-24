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

// LIR class
class LIR {
 public:
  LIR(Heap* heap, HIR* hir, Masm* masm);

  // Translates every HIRInstruction into the LIRInstruction
  void BuildInstructions();

  // Replace LIRValues with LIROperand
  void AssignRegisters(LIRInstruction* instr);

  // Generate machine code for linked-list of instructions
  void Generate(HIRBasicBlock* entry, int spill_count);

  // Debug printing
  void Print(char* buffer, uint32_t size);

  // Convert HIR instruction into LIR instruction
  inline LIRInstruction* Cast(HIRInstruction* instr);

  // Adds instruction to linked list
  inline void AddInstruction(LIRInstruction* instr);
  inline int instruction_index() {
    int id = instruction_index_;
    instruction_index_ += 2;
    return id;
  }

  inline Heap* heap() { return heap_; }
  inline HIR* hir() { return hir_; }
  inline Masm* masm() { return masm_; }

 private:
  LIRInstruction* first_instruction_;
  LIRInstruction* last_instruction_;

  int instruction_index_;

  Heap* heap_;
  HIR* hir_;
  Masm* masm_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_H_
