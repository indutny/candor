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

#ifndef _SRC_FULLGEN_INSTRUCTIONS_H
#define _SRC_FULLGEN_INSTRUCTIONS_H

#include "ast.h"  // AstNode
#include "macroassembler.h"
#include "zone.h"
#include "utils.h"

namespace candor {
namespace internal {

// Forward declarations
class Fullgen;
class ScopeSlot;
class FInstruction;

typedef ZoneList<FInstruction*> FInstructionList;

#define FULLGEN_INSTRUCTION_TYPES(V) \
    V(Nop) \
    V(Chi) \
    V(Nil) \
    V(Label) \
    V(Entry) \
    V(Return) \
    V(Function) \
    V(Literal) \
    V(BinOp) \
    V(Not) \
    V(Store) \
    V(StoreContext) \
    V(StoreProperty) \
    V(Load) \
    V(LoadContext) \
    V(LoadProperty) \
    V(DeleteProperty) \
    V(AllocateObject) \
    V(AllocateArray) \
    V(Clone) \
    V(Typeof) \
    V(Sizeof) \
    V(Keysof) \
    V(If) \
    V(Goto) \
    V(Break) \
    V(Continue) \
    V(StoreArg) \
    V(StoreVarArg) \
    V(LoadArg) \
    V(LoadVarArg) \
    V(AlignCode) \
    V(AlignStack) \
    V(CollectGarbage) \
    V(GetStackTrace) \
    V(Call)

#define FULLGEN_INSTRUCTION_ENUM(V) \
    k##V,

class FInstruction : public ZoneObject {
 public:
  enum Type {
    FULLGEN_INSTRUCTION_TYPES(FULLGEN_INSTRUCTION_ENUM)
    kNone
  };

  explicit FInstruction(Type type);

  static inline const char* TypeToStr(Type type);
  inline FInstruction* SetResult(FOperand* op);
  inline FInstruction* AddArg(FOperand* op);
  inline AstNode* ast();
  inline void ast(AstNode* ast);
  inline Type type();

  virtual void Print(PrintBuffer* p);
  virtual void Init(Fullgen* f);
  virtual void Generate(Masm* masm) = 0;

  int id;
  FOperand* result;
  FOperand* inputs[3];

 protected:
  Fullgen* f_;
  AstNode* ast_;
  Type type_;
  int input_count_;
};

#undef FULLGEN_INSTRUCTION_ENUM

#define FULLGEN_DEFAULT_METHODS(V) \
    void Generate(Masm* masm); \
    static inline F##V* Cast(FInstruction* instr) { \
      assert(instr->type() == k##V); \
      return reinterpret_cast<F##V*>(instr); \
    }

class FNop : public FInstruction {
 public:
  FNop() : FInstruction(kNop) {
  }

  FULLGEN_DEFAULT_METHODS(Nop)
};

class FNil : public FInstruction {
 public:
  FNil() : FInstruction(kNil) {
  }

  FULLGEN_DEFAULT_METHODS(Nil)
};

class FLabel : public FInstruction {
 public:
  FLabel() : FInstruction(kLabel), label(new Label()) {
  }

  explicit FLabel(Label* l) : FInstruction(kLabel), label(l) {
  }

  FULLGEN_DEFAULT_METHODS(Label)

  Label* label;
};

class FEntry : public FInstruction {
 public:
  explicit FEntry(int context_slots) : FInstruction(kEntry),
                                       context_slots_(context_slots) {
  }

  inline int stack_slots();
  inline void stack_slots(int stack_slots);
  FULLGEN_DEFAULT_METHODS(Entry)

 protected:
  int context_slots_;
  int stack_slots_;
};

class FReturn : public FInstruction {
 public:
  FReturn() : FInstruction(kReturn) {
  }

  FULLGEN_DEFAULT_METHODS(Return)
};

class FFunction : public FInstruction {
 public:
  FFunction(AstNode* ast, int argc) : FInstruction(kFunction),
                                      body(NULL),
                                      entry(NULL),
                                      argc_(argc),
                                      root_ast_(ast) {
  }

  FULLGEN_DEFAULT_METHODS(Function)

  inline AstNode* root_ast();
  FLabel* body;
  FEntry* entry;

 protected:
  int argc_;
  AstNode* root_ast_;
};

class FLiteral : public FInstruction {
 public:
  FLiteral(AstNode::Type type, ScopeSlot* slot) : FInstruction(kLiteral),
                                                  type_(type),
                                                  slot_(slot) {
  }

  FULLGEN_DEFAULT_METHODS(Literal)

 protected:
  AstNode::Type type_;
  ScopeSlot* slot_;
};

class FBinOp : public FInstruction {
 public:
  explicit FBinOp(BinOp::BinOpType sub_type) : FInstruction(kBinOp),
                                               sub_type_(sub_type) {
  }

  FULLGEN_DEFAULT_METHODS(BinOp)

 protected:
  BinOp::BinOpType sub_type_;
};

class FNot : public FInstruction {
 public:
  FNot() : FInstruction(kNot) {
  }

  FULLGEN_DEFAULT_METHODS(Not)
};

class FStore : public FInstruction {
 public:
  FStore() : FInstruction(kStore) {
  }

  FULLGEN_DEFAULT_METHODS(Store)
};

class FStoreContext : public FInstruction {
 public:
  FStoreContext() : FInstruction(kStoreContext) {
  }

  FULLGEN_DEFAULT_METHODS(StoreContext)
};

class FStoreProperty : public FInstruction {
 public:
  FStoreProperty() : FInstruction(kStoreProperty) {
  }

  FULLGEN_DEFAULT_METHODS(StoreProperty)
};

class FLoad : public FInstruction {
 public:
  FLoad() : FInstruction(kLoad) {
  }

  FULLGEN_DEFAULT_METHODS(Load)
};

class FLoadContext : public FInstruction {
 public:
  FLoadContext() : FInstruction(kLoadContext) {
  }

  FULLGEN_DEFAULT_METHODS(LoadContext)
};

class FLoadProperty : public FInstruction {
 public:
  FLoadProperty() : FInstruction(kLoadProperty) {
  }

  FULLGEN_DEFAULT_METHODS(LoadProperty)
};

class FDeleteProperty : public FInstruction {
 public:
  FDeleteProperty() : FInstruction(kDeleteProperty) {
  }

  FULLGEN_DEFAULT_METHODS(DeleteProperty)
};

class FAllocateObject : public FInstruction {
 public:
  explicit FAllocateObject(int size)
      : FInstruction(kAllocateObject),
        size_(RoundUp(PowerOfTwo(size + 1), 64)) {
  }

  FULLGEN_DEFAULT_METHODS(AllocateObject)

 protected:
  int size_;
};

class FAllocateArray : public FInstruction {
 public:
  explicit FAllocateArray(int size)
      : FInstruction(kAllocateArray),
        size_(RoundUp(PowerOfTwo(size + 1), 64)) {
  }

  FULLGEN_DEFAULT_METHODS(AllocateArray)

 protected:
  int size_;
};

class FChi : public FInstruction {
 public:
  FChi() : FInstruction(kChi) {
  }

  FULLGEN_DEFAULT_METHODS(Chi)
};

class FClone : public FInstruction {
 public:
  FClone() : FInstruction(kClone) {
  }

  FULLGEN_DEFAULT_METHODS(Clone)
};

class FSizeof : public FInstruction {
 public:
  FSizeof() : FInstruction(kSizeof) {
  }

  FULLGEN_DEFAULT_METHODS(Sizeof)
};

class FTypeof : public FInstruction {
 public:
  FTypeof() : FInstruction(kTypeof) {
  }

  FULLGEN_DEFAULT_METHODS(Typeof)
};

class FKeysof : public FInstruction {
 public:
  FKeysof() : FInstruction(kKeysof) {
  }

  FULLGEN_DEFAULT_METHODS(Keysof)
};

class FBreak : public FInstruction {
 public:
  explicit FBreak(FLabel* label) : FInstruction(kBreak), label_(label) {
  }

  void Print(PrintBuffer* p);
  FULLGEN_DEFAULT_METHODS(Break)

 protected:
  FLabel* label_;
};

class FContinue : public FInstruction {
 public:
  explicit FContinue(FLabel* label) : FInstruction(kContinue), label_(label) {
  }

  void Print(PrintBuffer* p);
  FULLGEN_DEFAULT_METHODS(Continue)

 protected:
  FLabel* label_;
};

class FIf : public FInstruction {
 public:
  FIf(FLabel* t, FLabel* f) : FInstruction(kIf), t_(t), f_(f) {
  }

  void Print(PrintBuffer* p);
  FULLGEN_DEFAULT_METHODS(If)

 protected:
  FLabel* t_;
  FLabel* f_;
};

class FGoto : public FInstruction {
 public:
  explicit FGoto(FLabel* label) : FInstruction(kGoto), label_(label) {
  }

  void Print(PrintBuffer* p);
  FULLGEN_DEFAULT_METHODS(Goto)

 protected:
  FLabel* label_;
};

class FStoreArg : public FInstruction {
 public:
  FStoreArg() : FInstruction(kStoreArg) {
  }

  FULLGEN_DEFAULT_METHODS(StoreArg)
};

class FStoreVarArg : public FInstruction {
 public:
  FStoreVarArg() : FInstruction(kStoreVarArg) {
  }

  FULLGEN_DEFAULT_METHODS(StoreVarArg)
};

class FLoadArg : public FInstruction {
 public:
  FLoadArg() : FInstruction(kLoadArg) {
  }

  FULLGEN_DEFAULT_METHODS(LoadArg)
};

class FLoadVarArg : public FInstruction {
 public:
  FLoadVarArg() : FInstruction(kLoadVarArg) {
  }

  FULLGEN_DEFAULT_METHODS(LoadVarArg)
};

class FAlignCode : public FInstruction {
 public:
  FAlignCode() : FInstruction(kAlignCode) {
  }

  FULLGEN_DEFAULT_METHODS(AlignCode)
};

class FAlignStack : public FInstruction {
 public:
  FAlignStack() : FInstruction(kAlignStack) {
  }

  FULLGEN_DEFAULT_METHODS(AlignStack)
};

class FCollectGarbage : public FInstruction {
 public:
  FCollectGarbage() : FInstruction(kCollectGarbage) {
  }

  FULLGEN_DEFAULT_METHODS(CollectGarbage)
};

class FGetStackTrace : public FInstruction {
 public:
  FGetStackTrace() : FInstruction(kGetStackTrace) {
  }

  FULLGEN_DEFAULT_METHODS(GetStackTrace)
};

class FCall : public FInstruction {
 public:
  FCall() : FInstruction(kCall) {
  }

  FULLGEN_DEFAULT_METHODS(Call)
};

#undef FULLGEN_DEFAULT_METHODS
}  // internal
}  // candor

#endif  // _SRC_FULLGEN_INSTRUCTIONS_H
