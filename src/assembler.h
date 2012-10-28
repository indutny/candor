#ifndef _SRC_ASSEMBLER_H_
#define _SRC_ASSEMBLER_H_

#if CANDOR_ARCH_x64
#include "x64/assembler-x64.h"
#include "x64/assembler-x64-inl.h"
#elif CANDOR_ARCH_ia32
#include "ia32/assembler-ia32.h"
#include "ia32/assembler-ia32-inl.h"
#endif

namespace candor {
namespace internal {

class Label {
 public:
  Label() : pos_(0) {
  }

  inline void AddUse(Assembler* a, RelocationInfo* use);

 private:
  inline void relocate(uint32_t offset);
  inline void use(Assembler* a, uint32_t offset);

  uint32_t pos_;
  List<RelocationInfo*, EmptyClass> uses_;

  friend class Assembler;
};

} // namespace internal
} // namespace candor

#endif // _SRC_ASSEMBLER_H_
