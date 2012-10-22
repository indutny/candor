#ifndef _SRC_LIR_BUILDER_H_
#define _SRC_LIR_BUILDER_H_

#if CANDOR_ARCH_x64
#include "x64/lir-builder-x64.h"
#include "x64/lir-builder-x64-inl.h"
#elif CANDOR_ARCH_ia32
#include "ia32/lir-builder-ia32.h"
#include "ia32/lir-builder-ia32-inl.h"
#endif

#endif // _SRC_LIR_BUILDER_H_
