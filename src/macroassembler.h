#ifndef _SRC_MACROASSEMBLER_H_
#define _SRC_MACROASSEMBLER_H_

#if __ARCH == x64
#include "x64/macroassembler-x64.h"
#include "x64/macroassembler-x64-inl.h"
#else
#include "ia32/macroassembler-ia32.h"
#include "ia32/macroassembler-ia32-inl.h"
#endif

#endif // _SRC_MACROASSEMBLER_H_
