#ifndef _SRC_FULLGEN_INL_H_
#define _SRC_FULLGEN_INL_H_

#include "fullgen.h"
#include "fullgen-instructions.h"
#include "fullgen-instructions-inl.h"
#include "heap.h"

namespace candor {
namespace internal {

inline int Fullgen::instr_id() {
  return instr_id_ += 2;
}


inline FLabel* Fullgen::loop_start() {
  return loop_start_;
}


inline void Fullgen::loop_start(FLabel* loop_start) {
  loop_start_ = loop_start;
}


inline FLabel* Fullgen::loop_end() {
  return loop_end_;
}


inline void Fullgen::loop_end(FLabel* loop_end) {
  loop_end_ = loop_end;
}


inline SourceMap* Fullgen::source_map() {
  return source_map_;
}


inline void Fullgen::Print(char* out, int32_t size) {
  PrintBuffer p(out, size);
  Print(&p);
}

} // namespace internal
} // namespace candor

#endif // _SRC_FULLGEN_INL_H_
