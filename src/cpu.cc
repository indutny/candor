#include "cpu.h"
#include "zone.h" // Zone
#include "code-space.h" // CodeSpace

#include "macroassembler.h"

#include <stdint.h> // uint32_t
#include <string.h> // memcpy
#include <sys/types.h> // off_t

namespace candor {
namespace internal {

CPU::CPUFeatures CPU::cpu_features_;
bool CPU::probed_ = false;


typedef char* (*CPUProbeCallback)();

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

  CodePage page(a.length());
  char* code = page.Allocate(a.length());
  memcpy(code, a.buffer(), a.length());
  a.Relocate(code);

  int32_t features = reinterpret_cast<off_t>(
      reinterpret_cast<CPUProbeCallback>(code)());

  cpu_features_.SSE4_1 = (features & (1 << 19)) != 0;
  probed_ = true;
}

} // internal
} // candor
