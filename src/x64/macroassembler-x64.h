#ifndef _SRC_X64_MARCOASSEMBLER_H_
#define _SRC_X64_MARCOASSEMBLER_H_

#include "assembler-x64.h"
#include "ast.h" // AstNode
#include "heap.h" // HeapValue

namespace dotlang {

class Masm : public Assembler {
 public:
  Masm(Heap* heap) : result_(scratch), slot_(NULL), heap_(heap) {
  }

  void Allocate(Register result,
                Register result_end,
                uint32_t size,
                Register scratch,
                Label* runtime_allocate);
  void AllocateContext(uint32_t slots);

  inline Register result() { return result_; }
  inline Operand* slot() { return slot_; }

  Register result_;
  Operand* slot_;

 protected:
  Heap* heap_;
};

} // namespace dotlang

#endif // _SRC_X64_MARCOASSEMBLER_H_
