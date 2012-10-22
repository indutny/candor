#ifndef _SRC_LIR_H_
#define _SRC_LIR_H_

#include "hir.h"
#include "hir-inl.h"
#include "lir-builder.h"
#include "lir-instructions.h"
#include "zone.h" // Zone
#include "utils.h" // Lists and etc

namespace candor {
namespace internal {

// Forward-declarations
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

  void FlattenBlocks();
  void GenerateInstructions();
  void ComputeLocalLiveSets();
  void ComputeGlobalLiveSets();

  void Add(LInstruction* instr);

  inline int instr_id();

  inline void Print(PrintBuffer* p);
  inline void Print(char* out, int32_t size);

 private:
  HIRGen* hir_;
  int instr_id_;

  HIRBlockList blocks_;
  LInstructionList instructions_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_LIR_H_
