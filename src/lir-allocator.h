#ifndef _SRC_LIR_ALLOCATOR_H_
#define _SRC_LIR_ALLOCATOR_H_

#include "hir.h" // HIRValue

#include "utils.h"
#include "zone.h" // ZoneObject

namespace candor {
namespace internal {

// Forward declarations
class LIROperand;
class LIRInterval;

typedef ZoneList<LIROperand*> LIROperandList;
typedef ZoneList<LIRInterval*> LIRIntervalList;

// Operand for LIRInstruction
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

// parent:   ----------- |
// children:             | ------- | -------
class LIRInterval : public ZoneObject {
 public:
  LIRInterval(int start, int end) : start_(start),
                                    end_(end),
                                    operand_(NULL),
                                    parent_(NULL) {
  }

  inline LIRInterval* Split(int pos);

  inline int start() { return start_; }
  inline void start(int start) { start_ = start; }
  inline int end() { return end_; }
  inline void end(int end) { end_ = end; }
  inline LIROperand* operand() { return operand_; }
  inline void operand(LIROperand* operand) { operand_ = operand; }

  inline LIRInterval* parent() { return parent_; }
  inline void parent(LIRInterval* parent) { parent_ = parent; }
  inline LIRIntervalList* children() { return &children_; }

 private:
  int start_;
  int end_;
  LIROperand* operand_;

  LIRInterval* parent_;
  LIRIntervalList children_;
};

// LIRValue (Virtual Register)
class LIRValue : public LIROperand {
 public:
  LIRValue(HIRValue* hir, int start, int end) : LIROperand(kVirtual, -1),
                                                interval_(start, end),
                                                hir_(hir) {
  }

  inline LIRInterval* interval() { return &interval_; }
  inline HIRValue* hir() { return hir_; }

 private:
  LIRInterval interval_;
  HIRValue* hir_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_ALLOCATOR_H_
