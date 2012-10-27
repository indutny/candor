#include "test.h"
#include <parser.h>
#include <ast.h>
#include <hir.h>
#include <hir-inl.h>
#include <lir.h>
#include <lir-inl.h>

TEST_START(lir)
  // Simple assignments
  LIR_TEST("pass = 1\n"
           "while (i < 10) {\n"
           "  i++\n"
           "}\n"
           "return i + pass",
           "# Block 0\n"
           "i0 = Entry\n"
           "i2 = Literal[1]\n"
           "i4 = Literal[1]\n"
           "i6 = Return(i2)\n")
TEST_END(lir)
