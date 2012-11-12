#include "fullgen.h"
#include "fullgen-inl.h"
#include "code-space.h"
#include "heap.h"
#include "heap-inl.h"
#include "macroassembler.h"

namespace candor {
namespace internal {

Fullgen::Fullgen(CodeSpace* space, SourceMap* map)
    : Visitor<FInstruction>(kPreorder),
      space_(space),
      root_(space->heap()),
      loop_start_(NULL),
      loop_end_(NULL),
      source_map_(map) {
}


void Fullgen::Generate(AstNode* ast) {
}


void Fullgen::Print(PrintBuffer* p) {
  p->Print("## Fullgen\n");
  p->Print("## Fullgen ends\n");
}


void FOperand::Print(PrintBuffer* p) {
  switch (type_) {
   case kStack: p->Print("s[%d]", index_); break;
   case kContext: p->Print("c[%d:%d]", index_, depth_); break;
   case kRegister: p->Print("%s", RegisterNameByIndex(index_)); break;
   default: UNEXPECTED
  }
}

} // namespace internal
} // namespace candor
