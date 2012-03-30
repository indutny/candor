#ifndef _SRC_LIR_H_
#define _SRC_LIR_H_

#include "zone.h"

namespace candor {
namespace internal {

// Forward declarations
class Heap;
class HIR;
class HIRInstruction;
class LIRInstruction;

class LIROperand : public ZoneObject {
 public:
  enum Type {
    kSpill,
    kRegister
  };

  LIROperand(Type type) : type_(type) {
  }

  inline Type type() { return type_; }
  inline bool is(Type type) { return type_ == type; }

 private:
  Type type_;
  int value_;
};

class LIR {
 public:
  LIR(Heap* heap, HIR* hir);

  LIRInstruction* Cast(HIRInstruction* instr);

  char* Generate();

  inline Heap* heap() { return heap_; }
  inline HIR* hir() { return hir_; }

 private:
  Heap* heap_;
  HIR* hir_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_H_
