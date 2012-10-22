#ifndef _SRC_LIR_INSTRUCTIONS_H_
#define _SRC_LIR_INSTRUCTIONS_H_

#if CANDOR_ARCH_x64
#include "x64/lir-instructions-x64.h"
#include "x64/lir-instructions-x64-inl.h"
#elif CANDOR_ARCH_ia32
#include "ia32/lir-instructions-ia32.h"
#include "ia32/lir-instructions-ia32-inl.h"
#endif

#endif // _SRC_LIR_INSTRUCTIONS_H_
