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

  // XXX: I don't like that cast, though it seems to be correct solution
  reinterpret_cast<Masm*>(&a)->ProbeCPU();

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
