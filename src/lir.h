#ifndef _SRC_LIR_H_
#define _SRC_LIR_H_

#if CANDOR_ARCH_x64
#include "x64/lir-x64.h"
#elif CANDOR_ARCH_ia32
#include "ia32/lir-ia32.h"
#endif

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
class LLabel;
class LGap;
class LRange;
class LUse;
typedef ZoneList<LInterval*> LIntervalList;
typedef ZoneList<LRange*> LRangeList;
typedef ZoneList<LUse*> LUseList;
typedef HashMap<NumberKey, LUse, ZoneObject> LUseMap;

class LRange : public ZoneObject {
 public:
  LRange(LInterval* interval, int start, int end) : interval_(interval),
                                                    start_(start),
                                                    end_(end) {
  }

  int FindIntersection(LRange* with);

  inline LInterval* interval();
  inline void interval(LInterval* interval);

  inline int start();
  inline void start(int start);
  inline int end();

 private:
  LInterval* interval_;
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

  Register ToRegister();
  Operand* ToOperand();

  inline void Print(PrintBuffer* p);

  inline bool IsEqual(LUse* use);
  inline bool is_virtual();
  inline bool is_register();
  inline bool is_stackslot();

  inline LInstruction* instr();
  inline Type type();
  inline LInterval* interval();
  inline void interval(LInterval* interval);

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

  LInterval(Type type, int index) : id(-1),
                                    type_(type),
                                    index_(index),
                                    fixed_(false),
                                    split_parent_(NULL) {
  }

  LUse* Use(LUse::Type type, LInstruction* instr);
  void AddRange(int start, int end);
  bool Covers(int pos);
  LUse* UseAt(int pos);
  LUse* UseAfter(int pos, LUse::Type = LUse::kAny);
  int FindIntersection(LInterval* with);
  LInterval* ChildAt(int pos);

  inline void Allocate(int reg);
  inline void Spill(int slot);
  inline void MarkFixed();
  inline bool IsFixed();
  inline bool IsEqual(LInterval* i);

  inline bool is_virtual();
  inline bool is_register();
  inline bool is_stackslot();

  inline int index();
  inline LRangeList* ranges();
  inline LUseList* uses();
  inline LInterval* split_parent();
  inline void split_parent(LInterval* split_parent);
  inline LIntervalList* split_children();

  inline Type type();
  inline int start();
  inline int end();

  inline void Print(PrintBuffer* p);

  int id;

 private:
  Type type_;
  int index_;
  LRangeList ranges_;
  LUseList uses_;
  bool fixed_;

  LInterval* split_parent_;
  LIntervalList split_children_;
};

class LIntervalShape {
 public:
  static int Compare(LInterval* a, LInterval* b);
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
  inline LLabel* label();
  inline ZoneList<LInstruction*>* instructions();

 private:
  HIRBlock* hir_;
  LLabel* label_;
  ZoneList<LInstruction*> instructions_;
};

#define LGEN_VISITOR(V) \
    void Visit##V(HIRInstruction* instr);

class LGen : public ZoneObject {
 public:
  LGen(HIRGen* hir, HIRBlock* root);

  void Generate(Masm* masm);

  void FlattenBlocks(HIRBlock* root);
  void GenerateInstructions();
  void ComputeLocalLiveSets();
  void ComputeGlobalLiveSets();
  void BuildIntervals();
  void ShuffleIntervals(LIntervalList* active,
                        LIntervalList* inactive,
                        LIntervalList* handled,
                        int pos);
  void WalkIntervals();
  void ResolveDataFlow();
  void TryAllocateFreeReg(LInterval* current);
  void AllocateBlockedReg(LInterval* current);
  void AllocateSpills();

  void VisitInstruction(HIRInstruction* instr);
  HIR_INSTRUCTION_TYPES(LGEN_VISITOR)

  inline LInstruction* Add(LInstruction* instr);
  inline LInstruction* Bind(LInstruction* instr);
  LInterval* CreateInterval(LInterval::Type type, int index);
  inline LInterval* CreateVirtual();
  inline LInterval* CreateRegister(Register reg);
  inline LInterval* CreateStackSlot(int index);
  inline LBlock* IsBlockStart(int pos);

  LInterval* ToFixed(HIRInstruction* instr, Register reg);
  void ResultFromFixed(LInstruction* instr, Register reg);
  LInterval* Split(LInterval* i, int pos);
  LGap* GetGap(int pos);
  void Spill(LInterval* interval);

  inline int instr_id();
  inline int interval_id();
  inline int virtual_index();

  void Print(PrintBuffer* p, bool extended = false);
  void PrintIntervals(PrintBuffer* p);
  inline void Print(char* out, int32_t size, bool extended = false);

 private:
  HIRGen* hir_;
  int instr_id_;
  int interval_id_;
  int virtual_index_;

  LBlock* current_block_;
  HIRInstruction* current_instruction_;

  HIRBlockList blocks_;
  LInterval* registers_[kLIRRegisterCount];
  LIntervalList intervals_;
  ZoneList<LInstruction*> instructions_;

  // Walk intervals data
  LIntervalList unhandled_;
  LIntervalList active_;
  LIntervalList inactive_;

  int spill_index_;
  LIntervalList unhandled_spills_;
  LIntervalList active_spills_;
  LIntervalList inactive_spills_;
  LIntervalList free_spills_;
};

#undef LGEN_VISITOR

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_H_
