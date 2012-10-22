#ifndef _SRC_LIR_H_
#define _SRC_LIR_H_

#include "lir-instructions.h"
#include "lir-instructions-inl.h"
#include "zone.h" // Zone
#include "utils.h" // Lists and etc

namespace candor {
namespace internal {

// Forward-declarations
class HIRGen;
class HIRBlock;
class LGen;
class LOperand;
class LRange;
class LUse;
typedef ZoneList<LOperand*> LOperandList;
typedef ZoneList<LRange*> LRangeList;
typedef ZoneList<LUse*> LUseList;

class LRange : public ZoneObject {
 public:
  LRange(LOperand* op, int start, int end);

  inline int start();
  inline int end();

 private:
  LOperand* op_;
  int start_;
  int end_;
};

class LRangeShape {
 public:
  static int Compare(LRange* a, LRange* b);
};

class LUse : public ZoneObject {
 public:
  enum Type {
    kAny,
    kRegister
  };

  LUse(LOperand* op, Type type, LInstruction* instr);

  inline LInstruction* instr();

 private:
  LOperand* op_;
  Type type_;
  LInstruction* instr_;
};

class LUseShape {
 public:
  static int Compare(LUse* a, LUse* b);
};

class LOperand : public ZoneObject {
 public:
  enum Type {
    kUnallocated,
    kRegister,
    kStackSlot
  };

  LOperand(int index, Type type);

  LUse* Use(LUse::Type type, LInstruction* instr);
  LRange* AddRange(int start, int end);

  inline bool is_unallocated();
  inline bool is_register();
  inline bool is_stackslot();

  inline int index();

 private:
  int index_;
  Type type_;
  LRangeList ranges_;
  LUseList uses_;
};

class LUnallocated : public LOperand {
 public:
  LUnallocated(int index);
};

class LRegister: public LOperand {
 public:
  LRegister(int index);
};

class LStackSlot : public LOperand {
 public:
  LStackSlot(int index);
};

class LGen : public ZoneObject {
 public:
  LGen(HIRGen* hir);

  void VisitBlock(HIRBlock* block);

  inline int block_id();
  inline int instr_id();

 private:
  int block_id_;
  int instr_id_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_H_
