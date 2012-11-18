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

#ifndef _SRC_FULLGEN_INSTRUCTION_INL_H_
#define _SRC_FULLGEN_INSTRUCTION_INL_H_

#include "fullgen.h"
#include "fullgen-inl.h"
#include "fullgen-instructions.h"
#include <assert.h>  // assert

namespace candor {
namespace internal {

inline FInstruction* FInstruction::SetResult(FOperand* op) {
  assert(result == NULL);
  result = op;

  return this;
}


inline FInstruction* FInstruction::AddArg(FOperand* op) {
  assert(input_count_ < 3);
  inputs[input_count_++] = op;

  return this;
}


inline AstNode* FInstruction::ast() {
  return ast_;
}


inline void FInstruction::ast(AstNode* ast) {
  ast_ = ast;
}


inline FInstruction::Type FInstruction::type() {
  return type_;
}

#define FULLGEN_INSTRUCTION_TYPE_TO_STR(V) \
    case k##V: return #V;

inline const char* FInstruction::TypeToStr(Type type) {
  switch (type) {
    FULLGEN_INSTRUCTION_TYPES(FULLGEN_INSTRUCTION_TYPE_TO_STR)
    default: UNEXPECTED break;
  }
  return "none";
}

#undef FULLGEN_INSTRUCTION_TYPE_TO_STR

inline AstNode* FFunction::root_ast() {
  return root_ast_;
}


inline int FEntry::stack_slots() {
  return stack_slots_;
}


inline void FEntry::stack_slots(int stack_slots) {
  stack_slots_ = stack_slots;
}

}  // namespace internal
}  // namespace candor

#endif  // _SRC_FULLGEN_INSTRUCTION_INL_H_
