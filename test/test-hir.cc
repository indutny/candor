#include "test.h"
#include <parser.h>
#include <ast.h>
#include <hir.h>

TEST_START(hir)
  // Simple assignments
  HIR_TEST("a = 1\nb = 1",
           "[Block#0 "
           "[LoadRoot @[0 [reg]]] "
           "[StoreLocal @[1 [st:0]] @[0 [reg]]] "
           "[LoadRoot @[2 [reg]]] "
           "[StoreLocal @[3 [st:1]] @[2 [reg]]] "
           "{}"
           "]")
  HIR_TEST("a = 2\na = a",
           "[Block#0 "
           "[LoadRoot @[0 [reg]]] "
           "[StoreLocal @[1 [st:0]] @[0 [reg]]] "
           "[LoadLocal @[1 [st:0]]] "
           "[StoreLocal @[2 [st:0]] @[1 [st:0]]] "
           "{}"
           "]")

  // Multiple blocks
  HIR_TEST("if (a) { a = 2 }\na",
           "[Block#0 "
           "[LoadLocal @[0 [st:0]]] "
           "[BranchBool @[0 [st:0]]] "
           "{1+2}] "

           "[Block#1 "
           "[LoadRoot @[1 [reg]]] "
           "[StoreLocal @[2 [st:0]] @[1 [reg]]] "
           "[Goto ] {3}] "

           "[Block#3 [LoadLocal @[3 [st:0]]] {}] "

           "[Block#2 [Goto ] {3}]")

  // Phi
  HIR_TEST("if (a) { a = 2 } else { a = 3 }\na", "")
TEST_END(hir)
