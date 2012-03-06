#include "cpu.h"
#include "zone.h" // Zone
#include "compiler.h" // Guard

#if __ARCH == x64
#include "x64/assembler-x64.h"
#include "x64/assembler-x64-inl.h"
#else
#include "ia32/assembler-ia32.h"
#endif

#include <stdint.h> // uint32_t
#include <sys/types.h> // off_t

namespace candor {
namespace internal {

CPU::CPUFeatures CPU::cpu_features_;
bool CPU::probed_ = false;

void CPU::Probe() {
  Zone z;
  Assembler a;

#define __ a.
  __ push(rbp);
  __ movq(rbp, rsp);

  __ push(rbx);
  __ push(rcx);
  __ push(rdx);

  __ movq(rax, Immediate(0x01));
  __ cpuid();
  __ movq(rax, rcx);

  __ pop(rdx);
  __ pop(rcx);
  __ pop(rbx);

  __ movq(rsp, rbp);
  __ pop(rbp);
  __ ret(0);
#undef __

  Guard g(a.buffer(), a.length());
  a.Relocate(g.buffer());

  int32_t features = reinterpret_cast<off_t>(g.AsFunction()(
        NULL, NULL, NULL, NULL));

  cpu_features_.SSE4_1 = (features & (1 << 19)) != 0;
  probed_ = true;
}

} // internal
} // candor
