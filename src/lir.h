#ifndef _SRC_LIR_H_
#define _SRC_LIR_H_

#include "lir-instructions.h"
#include "lir-instructions-inl.h"
#include "zone.h" // Zone
#include "utils.h" // Lists and etc

namespace candor {
namespace internal {
namespace lir {

// Forward-declarations
class Operand;
class Range;
class Use;
typedef ZoneList<Operand*> OperandList;
typedef ZoneList<Range*> RangeList;
typedef ZoneList<Use*> UseList;

class LOperand : public ZoneObject {
 public:
  enum Type {
    kUnallocated,
    kRegister,
    kStackSlot
  };

  LOperand(Type type);

  inline bool is_unallocated();
  inline bool is_register();
  inline bool is_stackslot();

 private:
  int id_;
  Type type_;
  RangeList ranges_;
  UseList uses_;
};

class LUnallocated : public LOperand {
 public:
  LUnallocated();
};

class LRegister: public LOperand {
 public:
  LRegister();
};

class LStackSlot : public LOperand {
 public:
  LStackSlot();
};

class LLiveRange : public ZoneObject {
 public:
  LLiveRange(int start, int end);
};

class LUse : public ZoneObject {
 public:
  enum Type {
    kAny,
    kRegister
  };

  LUse(Type type, LInstruction* instr);

 private:
  Type type_;
  LInstruction* instr_;
};

} // namespace lir
} // namespace internal
} // namespace candor

#endif // _SRC_LIR_H_
