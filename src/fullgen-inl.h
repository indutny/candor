#ifndef _SRC_FULLGEN_INL_H_
#define _SRC_FULLGEN_INL_H_

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif // _STDC_FORMAT_MACROS

#include <inttypes.h> // PRIu64

#include "fullgen.h"
#include "fullgen-instructions.h"
#include "fullgen-instructions-inl.h"
#include "heap.h"

#include <assert.h>

namespace candor {
namespace internal {

inline FInstruction* Fullgen::Add(FInstruction* instr) {
  instructions_.Push(instr);
  instr->Init(this);
  return instr;
}


inline FOperand* Fullgen::CreateOperand(ScopeSlot* slot) {
  if (slot->is_stack()) {
    return new FStackSlot(slot->index());
  } else if (slot->is_context()) {
    return new FContextSlot(slot->index(), slot->depth());
  } else {
    UNEXPECTED
  }
}


inline void Fullgen::EmptySlots() {
  while (free_slots_.length() > 0) free_slots_.Shift();
  stack_index_ = 0;
}


inline FOperand* Fullgen::GetSlot() {
  if (free_slots_.length() > 0) {
    // Return previously created slot
    return free_slots_.Pop();
  }

  // Create new slot
  FOperand* slot = new FStackSlot(
      current_function()->root_ast()->stack_slots() + stack_index_++);

  return slot;
}


inline void Fullgen::ReleaseSlot(FOperand* slot) {
  assert(slot != NULL);
  free_slots_.Push(slot);
}


inline int Fullgen::instr_id() {
  int res = instr_id_;
  instr_id_ += 2;
  return res;
}


inline FInstruction* Fullgen::GetNumber(uint64_t i) {
  AstNode* index = new AstNode(AstNode::kNumber);

  // Fast-case
  if (i < 10) {
    switch (i) {
     case 0: index->value("0"); break;
     case 1: index->value("1"); break;
     case 2: index->value("2"); break;
     case 3: index->value("3"); break;
     case 4: index->value("4"); break;
     case 5: index->value("5"); break;
     case 6: index->value("6"); break;
     case 7: index->value("7"); break;
     case 8: index->value("8"); break;
     case 9: index->value("9"); break;
     default: UNEXPECTED
    }
    index->length(1);

    return Visit(index);
  }

  char keystr[32];
  index->value(keystr);
  index->length(snprintf(keystr, sizeof(keystr), "%" PRIu64, i));

  FInstruction* r = Visit(index);
  r->ast(NULL);

  return r;
}


inline FFunction* Fullgen::current_function() {
  return current_function_;
}


inline void Fullgen::set_current_function(FFunction* current_function) {
  current_function_ = current_function;
}


inline SourceMap* Fullgen::source_map() {
  return source_map_;
}


inline void Fullgen::Print(char* out, int32_t size) {
  PrintBuffer p(out, size);
  Print(&p);
}


inline FOperand* FScopedSlot::operand() {
  return operand_;
}

} // namespace internal
} // namespace candor

#endif // _SRC_FULLGEN_INL_H_
