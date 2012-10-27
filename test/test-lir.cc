#include "test.h"
#include <parser.h>
#include <ast.h>
#include <hir.h>
#include <hir-inl.h>
#include <lir.h>
#include <lir-inl.h>

TEST_START(lir)
  // Simpliest
  LIR_TEST("return 1\n",
           "# Block 0\n"
           "0: Label\n"
           "2: Entry\n"
           "4: @r4:rax = Literal[1]\n"
           "6: @r0:rax = Move r4:rax\n"
           "8: Return @r0:rax\n\n")

  LIR_TEST("return 1 + 2\n",
           "# Block 0\n"
           "0: Label\n"
           "2: Entry\n"
           "4: @r4:rax = Literal[1]\n"
           "6: @r5:rbx = Literal[2]\n"
           "8: @r1:rbx = Move r5:rbx\n"
           "10: @r0:rax = Move r4:rax\n"
           "12: @r0:rax = BinOp @r0:rax, @r1:rbx\n"
           "14: r6:rax = Move @r0:rax\n"
           "16: @r0:rax = Move r6:rax\n"
           "18: Return @r0:rax\n\n")

  // Ifs
  LIR_TEST("if (true) { a = 1 } else { a = 2}\nreturn a\n",
           "# Block 0\n"
           "0: Label\n"
           "2: Entry\n"
           "4: @r4:rax = Literal[true]\n"
           "6: @r0:rax = Move r4:rax\n"
           "8: Branch @r0:rax (10), (18)\n"
           "\n"
           "# Block 1\n"
           "# in: , out: 6\n"
           "10: Label\n"
           "12: @r5:rax = Literal[1]\n"
           "14: r6:rax = Move r5:rax\n"
           "16: Goto (26)\n"
           "\n"
           "# Block 2\n"
           "# in: , out: 6\n"
           "18: Label\n"
           "20: @r7:rax = Literal[2]\n"
           "22: r6:rax = Move r7:rax\n"
           "\n"
           "# Block 3\n"
           "# in: 6, out: \n"
           "26: Label\n"
           "28: r8:rax = Phi r6:rax\n"
           "30: @r0:rax = Move r8:rax\n"
           "32: Return @r0:rax\n\n")
TEST_END(lir)
