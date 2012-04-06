#include "test.h"
#include <parser.h>
#include <ast.h>
#include <hir.h>
#include <lir.h>

TEST_START(hir)
  // Simple assignments
  HIR_TEST("a = 1\nb = 1",
           "[Block#0 {1,3||1,3} "
           "[Entry] "
           "[LoadRoot *[0 [imm 0x2]]] "
           "[StoreLocal *[1 [st:0]] *[0 [imm 0x2]]] "
           "[LoadRoot *[2 [imm 0x2]]] "
           "[StoreLocal *[3 [st:1]] *[2 [imm 0x2]]] "
           "[Return *[4 [imm 0x1]]] "
           "[]>*>[]]\n")
  HIR_TEST("a = 2\na = a",
           "[Block#0 {1||2} "
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
           "[Block#0 {0||0} "
           "[Entry] "
           "[LoadLocal *[0 [st:0]]] "
           "[BranchBool *[0 [st:0]]] "
           "[]>*>[1,2]]\n"

           "[Block#1 {0||2} "
           "[LoadRoot *[1 [imm 0x4]]] "
           "[StoreLocal *[0>2 [st:0]] *[1 [imm 0x4]]] "
           "[Goto] [0]>*>[3]]\n"

           "[Block#3 {2||3} @[2,0]:3 "
           "[LoadLocal *[3 [st:0]]] "
           "[Return *[4 [imm 0x1]]] "
           "[1,2]>*>[]]\n"

           "[Block#2 {0||0} [Goto] [0]>*>[3]]\n")

  HIR_TEST("if (a) { a = 2 } else { a = 3 }\na",
           "[Block#0 {0||0} "
           "[Entry] "
           "[LoadLocal *[0 [st:0]]] "
           "[BranchBool *[0 [st:0]]] "
           "[]>*>[1,2]]\n"

           "[Block#1 {0||2} "
           "[LoadRoot *[1 [imm 0x4]]] "
           "[StoreLocal *[0>2 [st:0]] *[1 [imm 0x4]]] "
           "[Goto] [0]>*>[3]]\n"

           "[Block#3 {2||5} @[2,4]:5 "
           "[LoadLocal *[5 [st:0]]] "
           "[Return *[6 [imm 0x1]]] "
           "[1,2]>*>[]]\n"

           "[Block#2 {0||4} "
           "[LoadRoot *[3 [imm 0x6]]] "
           "[StoreLocal *[0>4 [st:0]] *[3 [imm 0x6]]] "
           "[Goto] [0]>*>[3]]\n")

  HIR_TEST("if (a) { a = 2 } else { if (a) { a = 3 } else { a = 4 } }\na",
           "[Block#0 {0||0} "
           "[Entry] "
           "[LoadLocal *[0 [st:0]]] "
           "[BranchBool *[0 [st:0]]] "
           "[]>*>[1,2]]\n"

           "[Block#1 {0||2} "
           "[LoadRoot *[1 [imm 0x4]]] "
           "[StoreLocal *[0>2 [st:0]] *[1 [imm 0x4]]] "
           "[Goto] [0]>*>[6]]\n"

           "[Block#6 {2||9} @[2,8]:9 "
           "[LoadLocal *[9 [st:0]]] "
           "[Return *[10 [imm 0x1]]] "
           "[1,5]>*>[]]\n"

           "[Block#2 {0||3} "
           "[LoadLocal *[0>3 [st:0]]] "
           "[BranchBool *[0>3 [st:0]]] "
           "[0]>*>[3,4]]\n"

           "[Block#3 {3||5} "
           "[LoadRoot *[4 [imm 0x6]]] "
           "[StoreLocal *[3>5 [st:0]] *[4 [imm 0x6]]] "
           "[Goto] [2]>*>[5]]\n"

           "[Block#5 {5||8} @[5,7]:8 "
           "[Goto] [3,4]>*>[6]]\n"

           "[Block#4 {3||7} "
           "[LoadRoot *[6 [imm 0x8]]] "
           "[StoreLocal *[3>7 [st:0]] *[6 [imm 0x8]]] "
           "[Goto] [2]>*>[5]]\n")

  // While loop
  HIR_TEST("a = 0\nwhile (true) { b = a\na = 2 }\nreturn a",
           "[Block#0 {1||1} [Entry] "
           "[LoadRoot *[0 [imm 0x0]]] "
           "[StoreLocal *[1 [st:0]] *[0 [imm 0x0]]] "
           "[Goto] []>*>[1]]\n"

           "[Block#1 {1,3||3,6} @[1,5]:6 "
           "[LoadRoot *[2 [ctx -2:11]]] "
           "[Goto] [0,3]>*>[2]]\n"

           "[Block#2 {1||1} "
           "[BranchBool *[2 [ctx -2:11]]] [1]>*>[3,4]]\n"

           "[Block#3 {1,3||3,5} "
           "[LoadLocal *[6 [st:0]]] "
           "[StoreLocal *[3 [st:1]] *[6 [st:0]]] "
           "[LoadRoot *[4 [imm 0x4]]] "
           "[StoreLocal *[1>5 [st:0]] *[4 [imm 0x4]]] "
           "[Goto] [2]>*>[1]]\n"

           "[Block#4 {1||1} "
           "[LoadLocal *[6 [st:0]]] "
           "[Return *[6 [st:0]]] [2]>*>[]]\n")

  HIR_TEST("i = 0\nwhile(++i) { }\nreturn i",
           "[Block#0 {1||1} [Entry] "
           "[LoadRoot *[0 [imm 0x0]]] "
           "[StoreLocal *[1 [st:0]] *[0 [imm 0x0]]] "
           "[Goto] []>*>[1]]\n"

           "[Block#1 {1,3||3,5} @[1,4]:5 "
           "[LoadLocal *[5 [st:0]]] "
           "[LoadRoot *[2 [imm 0x2]]] "
           "[BinOp *[5 [st:0]] *[2 [imm 0x2]] *[3 [st:-1]]] "
           "[StoreLocal *[1>4 [st:0]] *[3 [st:-1]]] "
           "[Goto] [0,3]>*>[2]]\n"

           "[Block#2 {3,4||3,4} [BranchBool *[3 [st:-1]]] [1]>*>[3,4]]\n"

           "[Block#3 {3,4||3,4} [Goto] [2]>*>[1]]\n"

           "[Block#4 {3,4||3,4} "
           "[LoadLocal *[5 [st:0]]] [Return *[5 [st:0]]] "
           "[2]>*>[]]\n")

  // Nested while
  HIR_TEST("while(nil){ while(nil) {} }",
           "[Block#0 {||} [Entry] [Goto] []>*>[1]]\n"
           "[Block#1 {||} [LoadRoot *[0 [imm 0x1]]] [Goto] [0,8]>*>[2]]\n"
           "[Block#2 {||} [BranchBool *[0 [imm 0x1]]] [1]>*>[3,4]]\n"
           "[Block#3 {||} [Goto] [2]>*>[5]]\n"
           "[Block#5 {||} [LoadRoot *[1 [imm 0x1]]] [Goto] [3,7]>*>[6]]\n"
           "[Block#6 {||} [BranchBool *[1 [imm 0x1]]] [5]>*>[7,8]]\n"
           "[Block#7 {||} [Goto] [6]>*>[5]]\n"
           "[Block#8 {||} [Goto] [6]>*>[1]]\n"
           "[Block#4 {||} [Return *[2 [imm 0x1]]] [2]>*>[]]\n")
TEST_END(hir)
