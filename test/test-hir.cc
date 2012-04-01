#include "test.h"
#include <parser.h>
#include <ast.h>
#include <hir.h>
#include <lir.h>

TEST_START(hir)
  // Simple assignments
  HIR_TEST("a = 1\nb = 1",
           "[Block#0 {0,1,2,3,4} "
           "[Entry] "
           "[LoadRoot *[0 [imm 0x2]]] "
           "[StoreLocal *[1 [st:0]] *[0 [imm 0x2]]] "
           "[LoadRoot *[2 [imm 0x2]]] "
           "[StoreLocal *[3 [st:1]] *[2 [imm 0x2]]] "
           "[Return *[4 [imm 0x1]]] "
           "[]>*>[]]\n")
  HIR_TEST("a = 2\na = a",
           "[Block#0 {0,2,3} "
           "[Entry] "
           "[LoadRoot *[0 [imm 0x4]]] "
           "[StoreLocal *[1 [st:0]] *[0 [imm 0x4]]] "
           "[LoadLocal *[1 [st:0]]] "
           "[StoreLocal *[1>2 [st:0]] *[1 [st:0]]] "
           "[Return *[3 [imm 0x1]]] "
           "[]>*>[]]\n")

  // Multiple blocks and phi
  HIR_TEST("if (a) { a = 2 }\n"
           "// phi should be inserted here\na",
           "[Block#0 {0} "
           "[Entry] "
           "[LoadLocal *[0 [st:0]]] "
           "[BranchBool *[0 [st:0]]] "
           "[]>*>[1,2]]\n"

           "[Block#1 {1,2} "
           "[LoadRoot *[1 [imm 0x4]]] "
           "[StoreLocal *[0>2 [st:0]] *[1 [imm 0x4]]] "
           "[Goto] [0]>*>[3]]\n"

           "[Block#3 {1,3,4} @[2,0]:3 "
           "[LoadLocal *[3 [st:0]]] "
           "[Return *[4 [imm 0x1]]] "
           "[1,2]>*>[]]\n"

           "[Block#2 {0} [Nop] [Goto] [0]>*>[3]]\n")

  HIR_TEST("if (a) { a = 2 } else { a = 3 }\na",
           "[Block#0 {0} "
           "[Entry] "
           "[LoadLocal *[0 [st:0]]] "
           "[BranchBool *[0 [st:0]]] "
           "[]>*>[1,2]]\n"

           "[Block#1 {1,2} "
           "[LoadRoot *[1 [imm 0x4]]] "
           "[StoreLocal *[0>2 [st:0]] *[1 [imm 0x4]]] "
           "[Goto] [0]>*>[3]]\n"

           "[Block#3 {1,3,5,6} @[2,4]:5 "
           "[LoadLocal *[5 [st:0]]] "
           "[Return *[6 [imm 0x1]]] "
           "[1,2]>*>[]]\n"

           "[Block#2 {3,4} "
           "[LoadRoot *[3 [imm 0x6]]] "
           "[StoreLocal *[0>4 [st:0]] *[3 [imm 0x6]]] "
           "[Goto] [0]>*>[3]]\n")

  HIR_TEST("if (a) { a = 2 } else { if (a) { a = 3 } else { a = 4 } }\na",
           "[Block#0 {0} "
           "[Entry] "
           "[LoadLocal *[0 [st:0]]] "
           "[BranchBool *[0 [st:0]]] "
           "[]>*>[1,2]]\n"

           "[Block#1 {1,2} "
           "[LoadRoot *[1 [imm 0x4]]] "
           "[StoreLocal *[0>2 [st:0]] *[1 [imm 0x4]]] "
           "[Goto] [0]>*>[6]]\n"

           "[Block#6 {1,4,6,9,10} @[2,8]:9 "
           "[LoadLocal *[9 [st:0]]] "
           "[Return *[10 [imm 0x1]]] "
           "[1,5]>*>[]]\n"

           "[Block#2 {3} "
           "[LoadLocal *[0>3 [st:0]]] "
           "[BranchBool *[0>3 [st:0]]] "
           "[0]>*>[3,4]]\n"

           "[Block#3 {4,5} "
           "[LoadRoot *[4 [imm 0x6]]] "
           "[StoreLocal *[3>5 [st:0]] *[4 [imm 0x6]]] "
           "[Goto] [2]>*>[5]]\n"

           "[Block#5 {4,6,8} @[5,7]:8 "
           "[Goto] [3,4]>*>[6]]\n"

           "[Block#4 {6,7} "
           "[LoadRoot *[6 [imm 0x8]]] "
           "[StoreLocal *[3>7 [st:0]] *[6 [imm 0x8]]] "
           "[Goto] [2]>*>[5]]\n")
TEST_END(hir)
