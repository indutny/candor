#ifndef _SRC_LIR_ALLOCATOR_H_
#define _SRC_LIR_ALLOCATOR_H_

#include "utils.h"
#include "zone.h" // ZoneObject

namespace candor {
namespace internal {

// Forward declarations
class LIR;
class LIRAllocator;
class LIROperand;
class LIRLiveRange;
class LIRInterval;
class LIRValue;
class LIRInstruction;
class HIR;
class HIRBasicBlock;
class HIRValue;

typedef ZoneList<LIROperand*> LIROperandList;
typedef ZoneList<LIRLiveRange*> LIRRangeList;
typedef ZoneList<LIRInterval*> LIRIntervalList;
typedef ZoneList<LIRValue*> LIRValueList;

class LIROperand : public ZoneObject {
 public:
  enum Type {
    kAny,
    kVirtual,
    kInterval,
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
  inline bool is_interval() { return type_ == kInterval; }
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
    if (is_virtual() || is_interval()) {
      return this == op;
    }
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

  // See description of LIRInterval::FindIntersection
  inline int FindIntersection(LIRLiveRange* range);

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
  LIRUse(LIRInstruction* pos, LIROperand::Type kind) : pos_(pos),
                                                       kind_(kind),
                                                       operand_(NULL),
                                                       prev_(NULL),
                                                       next_(NULL) {
  }

  inline LIRInstruction* pos() { return pos_; }
  inline LIROperand::Type kind() { return kind_; }

  inline LIROperand* operand() { return operand_; }
  inline void operand(LIROperand* operand) { operand_ = operand; }

  inline LIRUse* prev() { return prev_; }
  inline void prev(LIRUse* prev) { prev_ = prev; }
  inline LIRUse* next() { return next_; }
  inline void next(LIRUse* next) { next_ = next; }

 private:
  LIRInstruction* pos_;
  LIROperand::Type kind_;
  LIROperand* operand_;

  LIRUse* prev_;
  LIRUse* next_;
};

class LIRIntervalShape {
 public:
  static int Compare(LIRInterval* a, LIRInterval* b);
};

class LIRInterval : public LIROperand {
 public:
  enum IntervalKind {
    kNormal,
    kFixed
  };

  LIRInterval(LIRValue* value) : LIROperand(kInterval, -1),
                                 value_(value),
                                 operand_(NULL),
                                 kind_(kNormal),
                                 first_range_(NULL),
                                 last_range_(NULL),
                                 first_use_(NULL),
                                 last_use_(NULL),
                                 parent_(NULL),
                                 enumerated_(false) {
  }

  // Creates new interval and links it with parent
  LIRInterval* SplitAt(int pos);

  // Returns interval or one of it's children that covers given position
  LIRInterval* ChildAt(int pos);

  // Add range to value's liveness interval's ranges
  // (should be called in bottom-up order, i.e. when traversing from the last
  //  instruction to the first one)
  void AddLiveRange(int start, int end);

  // Add use to uses list
  LIRUse* AddUse(LIRInstruction* pos, LIROperand::Type kind);

  // True if there is a range in this interval that covers specific position
  bool Covers(int pos);

  // Returns -1 if intervals doesn't intersect, otherwise returns the closest
  // intersection point of two intervals.
  int FindIntersection(LIRInterval* interval);

  // Split and spill interval in the point of intersection with the given one
  void SplitAndSpill(LIRAllocator* allocator, LIRInterval* interval);

  // Finds interval at specific position
  LIROperand* OperandAt(int pos, bool is_move=false);

  // Finds closest use after position
  LIRUse* NextUseAfter(LIROperand::Type kind, int pos);

  static inline LIRInterval* Cast(LIROperand* operand) {
    assert(operand->is_interval());
    return reinterpret_cast<LIRInterval*>(operand);
  }

  inline int start() {
    return first_range() == NULL ? -1 : first_range()->start();
  }
  inline int end() {
    return last_range() == NULL ? -1 : last_range()->end();
  }

  inline int rec_start() {
    if (first_range() != NULL)  return first_range()->start();
    if (children()->length() == 0) return -1;
    return children()->head()->value()->rec_start();
  }
  inline int rec_end() {
    if (children()->length() == 0) return end();
    return children()->tail()->value()->rec_end();
  }

  inline LIRValue* value() { return value_; }

  inline LIROperand* operand() { return operand_; }
  inline void operand(LIROperand* operand) { operand_ = operand; }

  inline IntervalKind kind() { return kind_; }
  inline void kind(IntervalKind kind) { kind_ = kind; }
  inline bool is_normal() { return kind_ == kNormal; }
  inline bool is_fixed() { return kind_ == kFixed; }

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

  inline bool enumerated() { return enumerated_; }
  inline void enumerated(bool enumerated) { enumerated_ = enumerated; }

 private:
  LIRValue* value_;
  LIROperand* operand_;

  IntervalKind kind_;

  LIRLiveRange* first_range_;
  LIRLiveRange* last_range_;

  LIRUse* first_use_;
  LIRUse* last_use_;

  LIRInterval* parent_;
  LIRIntervalList children_;

  bool enumerated_;
};

// LIRValue (Virtual Register)
class LIRValue : public LIROperand {
 public:
  LIRValue(HIRValue* hir) : LIROperand(kVirtual, -1),
                            interval_(this),
                            hir_(hir) {
  }

  // Replaces LIRValue with LIROperand
  static void ReplaceWithOperand(LIRInstruction* instr,
                                 LIROperand** operand,
                                 bool is_move = false);

  static inline LIRValue* Cast(LIROperand* operand) {
    assert(operand->is_virtual());
    return reinterpret_cast<LIRValue*>(operand);
  }

  inline LIRInterval* interval() { return &interval_; }
  inline HIRValue* hir() { return hir_; }

 private:
  LIRInterval interval_;
  HIRValue* hir_;
  bool enumerated_;
};

class LIRAllocator {
 public:
  enum FixedPosition {
    kFixedBefore,
    kFixedAfter
  };

  LIRAllocator(LIR* lir, HIR* hir, ZoneList<HIRBasicBlock*>* blocks)
      : lir_(lir),
        hir_(hir),
        blocks_(blocks),
        spill_count_(1) {
    Init();
  }

  // Initializer
  void Init();

  // Compute live_in, live_out for each block
  void ComputeLocalLiveSets();
  void ComputeGlobalLiveSets();

  // Traverses blocks in a post-order and creates live ranges for all
  // LIRValues (intervals).
  void BuildIntervals();

  // Walk all intervals and assign an operand to each of them
  void WalkIntervals();

  // Register allocation routines
  void AllocateReg(LIRInterval* interval);
  bool AllocateFreeReg(LIRInterval* interval);
  void AllocateBlockedReg(LIRInterval* interval);

  void SplitAndSpillIntersecting(LIRInterval* interval);

  // Insert movements on block edges
  void ResolveDataFlow();

  // Adds fixed interval (with specified register if operand != NULL) and
  // adds movement from (value) to it.
  LIRInterval* GetFixed(FixedPosition position,
                        LIRInstruction* instr,
                        LIRValue* value,
                        LIROperand* operand);

  inline void AddUnhandled(LIRInterval* interval);

  // Gets spill from free list or creates a new one, and assigns it to the
  // interval
  inline void AssignSpill(LIRInterval* inteval);
  inline void ReleaseSpill(LIROperand* spill);

  // Add move to parallel move before instruction (if not on block's edge)
  inline void AddMoveBefore(int pos, LIROperand* from, LIROperand* to);

  inline LIR* lir() { return lir_; }
  inline HIR* hir() { return hir_; }
  inline ZoneList<HIRBasicBlock*>* blocks() { return blocks_; }

  inline LIRValue** registers() { return registers_; }
  inline LIRIntervalList* unhandled() { return &unhandled_; }
  inline LIRIntervalList* active() { return &active_; }
  inline LIRIntervalList* inactive() { return &inactive_; }

  inline LIROperandList* available_spills() { return &available_spills_; }

  inline int spill_count() { return spill_count_; }

 private:
  LIR* lir_;
  HIR* hir_;
  ZoneList<HIRBasicBlock*>* blocks_;

  LIRValue* registers_[128];
  LIRIntervalList unhandled_;
  LIRIntervalList active_;
  LIRIntervalList inactive_;

  LIROperandList available_spills_;
  int spill_count_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_ALLOCATOR_H_
