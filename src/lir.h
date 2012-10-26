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
typedef HashMap<NumberKey, LUse, ZoneObject> LUseMap;

class LRange : public ZoneObject {
 public:
  LRange(LInterval* op, int start, int end);

  inline int start();
  inline void start(int start);
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
  inline Type type();
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

  LInterval(Type type, int index) : id(-1), type_(type), index_(index) {
  }

  LUse* Use(LUse::Type type, LInstruction* instr);
  LRange* AddRange(int start, int end);
  bool Covers(int pos);
  LUse* UseAt(int pos);

  inline bool is_virtual();
  inline bool is_register();
  inline bool is_stackslot();

  inline int index();
  inline LRangeList* ranges();
  inline LUseList* uses();

  inline void Print(PrintBuffer* p);

  int id;

 private:
  Type type_;
  int index_;
  LRangeList ranges_;
  LUseList uses_;
};

class LBlock : public ZoneObject {
 public:
  LBlock(HIRBlock* hir);

  inline void PrintHeader(PrintBuffer* p);

  LUseMap live_gen;
  LUseMap live_kill;
  LUseMap live_in;
  LUseMap live_out;

  int start_id;
  int end_id;
  inline HIRBlock* hir();

 private:
  HIRBlock* hir_;
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
  void BuildIntervals();

  void VisitInstruction(HIRInstruction* instr);
  HIR_INSTRUCTION_TYPES(LGEN_VISITOR)

  inline LInstruction* Add(int type);
  inline LInstruction* Bind(int type);
  inline LInterval* CreateInterval(LInterval::Type type, int index);
  inline LInterval* CreateVirtual();
  inline LInterval* CreateRegister(Register reg);
  inline LInterval* CreateStackSlot(int index);

  LInterval* ToFixed(HIRInstruction* instr, Register reg);
  LInterval* FromFixed(Register reg, HIRInstruction* instr);
  LInterval* FromFixed(Register reg, LInterval* interval);

  inline int instr_id();
  inline int interval_id();
  inline int virtual_index();

  void Print(PrintBuffer* p);
  void PrintIntervals(PrintBuffer* p);
  inline void Print(char* out, int32_t size);

 private:
  HIRGen* hir_;
  int instr_id_;
  int interval_id_;
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
