#ifndef _SRC_LIR_ALLOCATOR_H_
#define _SRC_LIR_ALLOCATOR_H_

#include "hir.h" // HIRValue

#include "utils.h"
#include "zone.h" // ZoneObject

namespace candor {
namespace internal {

// Forward declarations
class LIR;
class LIROperand;
class LIRLiveRange;
class LIRInterval;
class LIRValue;
class LIRInstruction;
class HIR;

typedef ZoneList<LIROperand*> LIROperandList;
typedef ZoneList<LIRLiveRange*> LIRRangeList;
typedef ZoneList<LIRInterval*> LIRIntervalList;
typedef ZoneList<LIRValue*> LIRValueList;

class LIROperand : public ZoneObject {
 public:
  enum Type {
    kVirtual,
    kRegister,
    kSpill,
    kImmediate
  };

  LIROperand(Type type, off_t value) : type_(type),
                                       value_(value),
                                       has_immediate_value_(false),
                                       immediate_value_(0) {
  }

  LIROperand(Type type, char* value) : type_(type),
                                       has_immediate_value_(false),
                                       immediate_value_(0) {
    value_ = reinterpret_cast<off_t>(value);
  }

  // Debug printing
  void Print(PrintBuffer* p);

  inline Type type() { return type_; }
  inline bool is_virtual() { return type_ == kVirtual; }
  inline bool is_register() { return type_ == kRegister; }
  inline bool is_spill() { return type_ == kSpill; }
  inline bool is_immediate() { return type_ == kImmediate; }

  inline bool has_immediate_value() { return has_immediate_value_; }
  inline off_t immediate_value() { return immediate_value_; }
  inline void immediate_value(off_t immediate_value) {
    has_immediate_value_ = true;
    immediate_value_ = immediate_value;
  }

  inline bool is_equal(LIROperand* op) {
    return !is_immediate() && type() == op->type() && value() == op->value();
  }

  inline off_t value() { return value_; }

 private:
  Type type_;
  off_t value_;
  bool has_immediate_value_;
  off_t immediate_value_;
};

class LIRLiveRange : public ZoneObject {
 public:
  LIRLiveRange(int start, int end) : start_(start),
                                     end_(end),
                                     prev_(NULL),
                                     next_(NULL) {
  }

  inline int start() { return start_; }
  inline void start(int start) { start_ = start; }
  inline int end() { return end_; }
  inline void end(int end) { end_ = end; }

  inline LIRLiveRange* prev() { return prev_; }
  inline void prev(LIRLiveRange* prev) { prev_ = prev; }
  inline LIRLiveRange* next() { return next_; }
  inline void next(LIRLiveRange* next) { next_ = next; }

 private:
  int start_;
  int end_;

  LIRLiveRange* prev_;
  LIRLiveRange* next_;
};

class LIRUse : public ZoneObject {
 public:
  LIRUse(LIRInstruction* pos, LIROperand::Type kind, LIROperand* value) :
      pos_(pos),
      kind_(kind),
      value_(value),
      prev_(NULL),
      next_(NULL) {
  }

  inline LIRInstruction* pos() { return pos_; }
  inline LIROperand::Type kind() { return kind_; }
  inline LIROperand* value() { return value_; }

  inline LIRUse* prev() { return prev_; }
  inline void prev(LIRUse* prev) { prev_ = prev; }
  inline LIRUse* next() { return next_; }
  inline void next(LIRUse* next) { next_ = next; }

 private:
  LIRInstruction* pos_;
  LIROperand::Type kind_;
  LIROperand* value_;

  LIRUse* prev_;
  LIRUse* next_;
};

class LIRIntervalShape {
 public:
  static int Compare(LIRInterval* a, LIRInterval* b);
};

class LIRInterval : public ZoneObject {
 public:
  LIRInterval(LIRValue* value) : value_(value),
                                 operand_(NULL),
                                 first_range_(NULL),
                                 last_range_(NULL),
                                 first_use_(NULL),
                                 last_use_(NULL),
                                 parent_(NULL) {
  }

  // Creates new interval and links it with parent
  LIRInterval* SplitAt(int pos);

  // Add range to value's liveness interval's ranges
  // (should be called in bottom-up order, i.e. when traversing from the last
  //  instruction to the first one)
  void AddLiveRange(int start, int end);

  // Add use to uses list
  void AddUse(LIRInstruction* pos,
              LIROperand::Type kind,
              LIROperand* operand);

  inline int start() {
    return first_range() == NULL ? 0 : first_range()->start();
  }
  inline int end() {
    return last_range() == NULL ? 0 : last_range()->end();
  }

  inline LIRValue* value() { return value_; }

  inline LIROperand* operand() { return operand_; }
  inline void operand(LIROperand* operand) { operand_ = operand; }

  inline LIRLiveRange* first_range() { return first_range_; }
  inline void first_range(LIRLiveRange* range) { first_range_ = range; }
  inline LIRLiveRange* last_range() { return last_range_; }
  inline void last_range(LIRLiveRange* range) { last_range_ = range; }

  inline LIRUse* first_use() { return first_use_; }
  inline void first_use(LIRUse* first_use) { first_use_ = first_use; }
  inline LIRUse* last_use() { return last_use_; }
  inline void last_use(LIRUse* last_use) { last_use_ = last_use; }

  inline LIRInterval* parent() { return parent_; }
  inline void parent(LIRInterval* parent) { parent_ = parent; }
  inline LIRIntervalList* children() { return &children_; }

 private:
  LIRValue* value_;
  LIROperand* operand_;

  LIRLiveRange* first_range_;
  LIRLiveRange* last_range_;

  LIRUse* first_use_;
  LIRUse* last_use_;

  LIRInterval* parent_;
  LIRIntervalList children_;
};

// LIRValue (Virtual Register)
class LIRValue : public LIROperand {
 public:
  LIRValue(HIRValue* hir) : LIROperand(kVirtual, -1),
                            interval_(this),
                            hir_(hir),
                            enumerated_(false) {
  }

  // Finds interval at specific position
  LIRInterval* FindInterval(int pos);

  inline LIRInterval* interval() { return &interval_; }
  inline HIRValue* hir() { return hir_; }

  inline bool enumerated() { return enumerated_; }
  inline void enumerated(bool enumerated) { enumerated_ = enumerated; }

 private:
  LIRInterval interval_;
  HIRValue* hir_;
  bool enumerated_;
};

class LIRAllocator {
 public:
  LIRAllocator(LIR* lir, HIR* hir) : lir_(lir), hir_(hir) {
  }

  // Initializer
  void Init();

  // Translates every HIRInstruction into the LIRInstruction
  void BuildInstructions();

  // Traverses blocks in a post-order and creates live ranges for all
  // LIRValues (intervals).
  void BuildIntervals();

  inline LIR* lir() { return lir_; }
  inline HIR* hir() { return hir_; }

  inline LIRValue** registers() { return registers_; }
  inline LIRIntervalList* intervals() { return &intervals_; }

 private:
  LIR* lir_;
  HIR* hir_;

  LIRValue* registers_[128];
  LIRIntervalList intervals_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_ALLOCATOR_H_
