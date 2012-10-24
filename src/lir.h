#ifndef _SRC_LIR_H_
#define _SRC_LIR_H_

#include "hir.h"
#include "hir-inl.h"
#include "macroassembler.h" // Register
#include "zone.h" // Zone
#include "utils.h" // Lists and etc

namespace candor {
namespace internal {

// Forward-declarations
class LGen;
class LInterval;
class LInstruction;
class LRange;
class LUse;
typedef ZoneList<LInterval*> LIntervalList;
typedef ZoneList<LRange*> LRangeList;
typedef ZoneList<LUse*> LUseList;

class LRange : public ZoneObject {
 public:
  LRange(LInterval* op, int start, int end);

  inline int start();
  inline int end();

 private:
  LInterval* op_;
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

  LUse(LInterval* op, Type type, LInstruction* instr) : interval_(op),
                                                        type_(type),
                                                        instr_(instr) {
  }

  inline void Print(PrintBuffer* p);

  inline LInstruction* instr();
  inline LInterval* interval();

 private:
  LInterval* interval_;
  Type type_;
  LInstruction* instr_;
};

class LUseShape {
 public:
  static int Compare(LUse* a, LUse* b);
};

class LInterval : public ZoneObject {
 public:
  enum Type {
    kVirtual,
    kRegister,
    kStackSlot
  };

  LInterval(Type type, int index) : type_(type), index_(index) {
  }


  LUse* Use(LUse::Type type, LInstruction* instr);
  LRange* AddRange(int start, int end);

  inline bool is_virtual();
  inline bool is_register();
  inline bool is_stackslot();

  inline int index();

  inline void Print(PrintBuffer* p);

 private:
  Type type_;
  int index_;
  LRangeList ranges_;
  LUseList uses_;
};

#define LGEN_VISITOR(V) \
    void Visit##V(HIRInstruction* instr); 

class LGen : public ZoneObject {
 public:
  LGen(HIRGen* hir);

  void FlattenBlocks();
  void GenerateInstructions();
  void ComputeLocalLiveSets();
  void ComputeGlobalLiveSets();

  void VisitInstruction(HIRInstruction* instr);
  HIR_INSTRUCTION_TYPES(LGEN_VISITOR)

  inline LInstruction* Add(int type);
  inline LInstruction* Bind(int type);
  inline LInterval* CreateInterval(LInterval::Type type, int index);
  inline LInterval* CreateVirtual();
  inline LInterval* CreateRegister(Register reg);
  inline LInterval* CreateStackSlot(int index);
  inline LInterval* ToFixed(HIRInstruction* instr, Register reg);
  inline LInterval* FromFixed(Register reg, HIRInstruction* instr);
  inline LInterval* FromFixed(Register reg, LInterval* interval);

  inline int instr_id();
  inline int virtual_index();

  inline void Print(PrintBuffer* p);
  inline void Print(char* out, int32_t size);

 private:
  HIRGen* hir_;
  int instr_id_;
  int virtual_index_;

  HIRBlock* current_block_;
  HIRInstruction* current_instruction_;

  HIRBlockList blocks_;
  LIntervalList intervals_;
  ZoneList<LInstruction*> instructions_;
};

#undef LGEN_VISITOR

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_H_
