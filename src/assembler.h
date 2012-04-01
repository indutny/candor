#ifndef _SRC_ASSEMBLER_H_
#define _SRC_ASSEMBLER_H_

#if CANDOR_ARCH_x64
#include "x64/assembler-x64.h"
#include "x64/assembler-x64-inl.h"
#elif CANDOR_ARCH_ia32
#include "ia32/assembler-ia32.h"
#include "ia32/assembler-ia32-inl.h"
#endif

#endif // _SRC_ASSEMBLER_H_
