/**
 * Copyright (c) 2012, Fedor Indutny.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _SRC_LIR_INSTRUCTIONS_H_
#define _SRC_LIR_INSTRUCTIONS_H_

#include "lir.h"
#include "lir-inl.h"
#include "hir-instructions.h"
#include "hir-instructions-inl.h"
#include "macroassembler.h"  // Label
#include "zone.h"
#include "utils.h"

namespace candor {
namespace internal {

// Forward declarations
class LInstruction;
class LBlock;
class ScopeSlot;
typedef ZoneList<LInstruction*> LInstructionList;

#define LIR_INSTRUCTION_SIMPLE_TYPES(V) \
    V(Nop) \
    V(Nil) \
    V(Move) \
    V(Return) \
    V(LoadContext) \
    V(StoreContext) \
    V(DeleteProperty) \
    V(LoadArg) \
    V(LoadVarArg) \
    V(StoreArg) \
    V(StoreVarArg) \
    V(AlignStack) \
    V(Not) \
    V(BinOp) \
    V(BinOpNumber) \
    V(Typeof) \
    V(Sizeof) \
    V(Keysof) \
    V(Clone) \
    V(Call) \
    V(CollectGarbage) \
    V(GetStackTrace) \
    V(Phi)

#define LIR_INSTRUCTION_TYPES(V) \
    V(Entry) \
    V(Label) \
    V(Gap) \
    V(Function) \
    V(Literal) \
    V(Branch) \
    V(BranchNumber) \
    V(LoadProperty) \
    V(StoreProperty) \
    V(AllocateObject) \
    V(AllocateArray) \
    V(Goto) \
    LIR_INSTRUCTION_SIMPLE_TYPES(V)

#define LIR_INSTRUCTION_ENUM(I) \
    k##I,

class LInstruction : public ZoneObject {
 public:
  enum Type {
    LIR_INSTRUCTION_TYPES(LIR_INSTRUCTION_ENUM)
    kNone
  };

  explicit LInstruction(Type type) : id(-1),
                                     type_(type),
                                     input_count_(0),
                                     scratch_count_(0),
                                     has_call_(NULL),
                                     block_(NULL),
                                     slot_(NULL),
                                     hir_(NULL),
                                     propagated_(NULL) {
    inputs[0] = NULL;
    inputs[1] = NULL;
    scratches[0] = NULL;
    scratches[1] = NULL;

    result = NULL;
  }

  inline LInstruction* AddArg(LInterval* arg, LUse::Type use_type);
  inline LInstruction* AddArg(LInstruction* arg, LUse::Type use_type);
  inline LInstruction* AddArg(HIRInstruction* arg, LUse::Type use_type);

  inline LInstruction* AddScratch(LInterval* scratch);

  inline LInstruction* SetResult(LInterval* res, LUse::Type use_type);
  inline LInstruction* SetResult(LInstruction* res, LUse::Type use_type);
  inline LInstruction* SetResult(HIRInstruction* res, LUse::Type use_type);
  inline LInstruction* Propagate(LUse* res);
  inline LInstruction* Propagate(HIRInstruction* res);

  inline LInstruction* SetSlot(ScopeSlot* slot);

  inline LInstruction* MarkHasCall() {
    has_call_ = true;
    return this;
  }
  inline bool HasCall() { return has_call_; }

  inline Type type() { return type_; }
  inline LBlock* block() { return block_; }
  inline void block(LBlock* block) { block_ = block; }
  int id;

  static inline const char* TypeToStr(Type type);

  virtual void Generate(Masm* masm) = 0;
  virtual void Print(PrintBuffer* p);

  int input_count() { return input_count_; }
  int result_count() { return result != NULL; }
  int scratch_count() { return scratch_count_; }

  inline ScopeSlot* slot() {
    assert(slot_ != NULL);
    return slot_;
  }
  inline HIRInstruction* hir() { return hir_; }
  inline void hir(HIRInstruction* hir) { hir_ = hir; }

  LUse* inputs[2];
  LUse* scratches[2];
  LUse* result;

 private:
  Type type_;
  int input_count_;
  int scratch_count_;
  bool has_call_;

  LBlock* block_;
  ScopeSlot* slot_;
  HIRInstruction* hir_;
  LUse* propagated_;
};

#undef LIR_INSTRUCTION_ENUM

#define INSTRUCTION_METHODS(Name) \
      void Generate(Masm* masm); \
      static inline L##Name* Cast(LInstruction* instr) { \
        assert(instr->type() == k##Name);\
        return reinterpret_cast<L##Name*>(instr); \
      }

class LEntry : public LInstruction {
 public:
  explicit LEntry(int context_slots) : LInstruction(kEntry),
                                       context_slots_(context_slots) {
  }

  INSTRUCTION_METHODS(Entry)

 private:
  int context_slots_;
};

class LLabel : public LInstruction {
 public:
  LLabel() : LInstruction(kLabel) {
    // Allocate label in zone
    label = new Label();
  }

  INSTRUCTION_METHODS(Label)

  Label* label;
};

class LGap : public LInstruction {
 public:
  enum PairStatus {
    kToMove,
    kBeingMoved,
    kMoved
  };

  class Pair : public ZoneObject {
   public:
    Pair(LUse* src, LUse* dst) : src_(src),
                                 dst_(dst),
                                 status(kToMove) {
    }

   private:
    LUse* src_;
    LUse* dst_;
    PairStatus status;

    friend class LGap;
  };

  typedef ZoneList<Pair*> PairList;

  explicit LGap(LInterval* tmp) : LInstruction(kGap),
                                  tmp_(tmp->Use(LUse::kAny, this)) {
  }

  INSTRUCTION_METHODS(Gap)

  inline void Add(LUse* src, LUse* dst);

  void Resolve();
  void Print(PrintBuffer* p);

 private:
  void MovePair(Pair* pair);

  LUse* tmp_;
  PairList unhandled_pairs_;
  PairList pairs_;
};

class LControlInstruction : public LInstruction {
 public:
  explicit LControlInstruction(Type type) : LInstruction(type),
                                            target_count_(0) {
    targets_[0] = NULL;
    targets_[1] = NULL;
  }

  void Print(PrintBuffer* p);

  inline void AddTarget(LLabel* target);
  inline LLabel* TargetAt(int i);

  inline int target_count() { return target_count_; }

  static inline LControlInstruction* Cast(LInstruction* instr);

 private:
  int target_count_;
  LLabel* targets_[2];
};

class LGoto : public LControlInstruction {
 public:
  LGoto() : LControlInstruction(kGoto) {
  }

  INSTRUCTION_METHODS(Goto)
};

class LBranch : public LControlInstruction {
 public:
  LBranch() : LControlInstruction(kBranch) {
  }

  INSTRUCTION_METHODS(Branch)
};

class LBranchNumber : public LControlInstruction {
 public:
  LBranchNumber() : LControlInstruction(kBranchNumber) {
  }

  INSTRUCTION_METHODS(BranchNumber)
};

class LAccessProperty : public LInstruction {
 public:
  explicit LAccessProperty(Type type) : LInstruction(type),
                                        monomorphic_prop_(false) {
  }

  inline void SetMonomorphicProperty();
  inline bool HasMonomorphicProperty();

 protected:
  bool monomorphic_prop_;
  AbsoluteAddress proto_ic, value_offset_ic, invalidate_ic;
};

class LLoadProperty : public LAccessProperty {
 public:
  LLoadProperty() : LAccessProperty(kLoadProperty) {
  }

  INSTRUCTION_METHODS(LoadProperty)
};

class LStoreProperty : public LAccessProperty {
 public:
  LStoreProperty() : LAccessProperty(kStoreProperty) {
  }

  INSTRUCTION_METHODS(StoreProperty)
};

class LFunction : public LInstruction {
 public:
  LFunction(LBlock* block, int arg_count) : LInstruction(kFunction),
                                            block_(block),
                                            arg_count_(arg_count) {
    assert(block_ != NULL);
  }

  INSTRUCTION_METHODS(Function)

 private:
  LBlock* block_;
  int arg_count_;
};

class LLiteral : public LInstruction {
 public:
  explicit LLiteral(ScopeSlot* slot) : LInstruction(kLiteral),
                                       root_slot_(slot) {
    assert(slot != NULL);
  }

  INSTRUCTION_METHODS(Literal)

 private:
  ScopeSlot* root_slot_;
};

class LAllocateObject : public LInstruction {
 public:
  explicit LAllocateObject(int size) : LInstruction(kAllocateObject),
                                       size_(size) {
  }

  INSTRUCTION_METHODS(AllocateObject)

 private:
  int size_;
};

class LAllocateArray : public LInstruction {
 public:
  explicit LAllocateArray(int size) : LInstruction(kAllocateArray),
                                      size_(size) {
  }

  INSTRUCTION_METHODS(AllocateArray)

 private:
  int size_;
};

#define DEFAULT_INSTR_IMPLEMENTATION(V) \
  class L##V : public LInstruction { \
    public: \
    L##V() : LInstruction(k##V) {} \
    INSTRUCTION_METHODS(V) \
  };

LIR_INSTRUCTION_SIMPLE_TYPES(DEFAULT_INSTR_IMPLEMENTATION)

#undef DEFAULT_INSTR_IMPLEMENTATION
#undef INSTRUCTION_METHODS

}  // namespace internal
}  // namespace candor

#endif  // _SRC_LIR_INSTRUCTIONS_H_
