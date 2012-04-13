#include "test.h"
#include <parser.h>
#include <ast.h>
#include <hir.h>
#include <lir.h>

TEST_START(hir)
  // Simple assignments
  HIR_TEST("a = 1\nb = 1",
           "[Block#0\n"
           "1: [Entry]\n"
           "3: *[0 [imm 0x2]] = [LoadRoot]\n"
           "5: *[1 [st:0]] = [StoreLocal *[0 [imm 0x2]]]\n"
           "9: *[2 [imm 0x2]] = [LoadRoot]\n"
           "11: *[3 [st:1]] = [StoreLocal *[2 [imm 0x2]]]\n"
           "15: [Return *[4 [imm 0x1]]]\n"
           "[]>*>[]]\n\n")
  HIR_TEST("a = 2\na = a",
           "[Block#0\n"
           "1: [Entry]\n"
           "3: *[0 [imm 0x4]] = [LoadRoot]\n"
           "5: *[1 [st:0]] = [StoreLocal *[0 [imm 0x4]]]\n"
           "11: *[2 [st:0]] = [StoreLocal *[1 [st:0]]]\n"
           "15: [Return *[3 [imm 0x1]]]\n"
           "[]>*>[]]\n\n")

  // Multiple blocks and phi
  HIR_TEST("if (a) { a = 2 }\n"
           "// phi should be inserted here\na",
           "[Block#0\n"
           "1: [Entry]\n"
           "5: [BranchBool *[0 [st:0]]]\n"
           "[]>*>[1,2]]\n\n"

           "[Block#1\n"
           "7: *[1 [imm 0x4]] = [LoadRoot]\n"
           "9: *[2 [st:0]] = [StoreLocal *[1 [imm 0x4]]]\n"
           "13: [Goto]\n"
           "[0]>*>[3]]\n\n"

           "[Block#3 @[2,0]:3\n"
           "17: [Return *[4 [imm 0x1]]]\n"
           "[1,2]>*>[]]\n\n"

           "[Block#2\n"
           "19: [Goto]\n"
           "[0]>*>[3]]\n\n")

  HIR_TEST("if (a) { a = 2 } else { a = 3 }\na",
           "[Block#0\n"
           "1: [Entry]\n"
           "5: [BranchBool *[0 [st:0]]]\n"
           "[]>*>[1,2]]\n\n"

           "[Block#1\n"
           "7: *[1 [imm 0x4]] = [LoadRoot]\n"
           "9: *[2 [st:0]] = [StoreLocal *[1 [imm 0x4]]]\n"
           "13: [Goto]\n"
           "[0]>*>[3]]\n\n"

           "[Block#3 @[2,4]:5\n"
           "17: [Return *[6 [imm 0x1]]]\n"
           "[1,2]>*>[]]\n\n"

           "[Block#2\n"
           "19: *[3 [imm 0x6]] = [LoadRoot]\n"
           "21: *[4 [st:0]] = [StoreLocal *[3 [imm 0x6]]]\n"
           "25: [Goto]\n"
           "[0]>*>[3]]\n\n")

  HIR_TEST("if (a) { a = 2 } else { if (a) { a = 3 } else { a = 4 } }\na",
           "[Block#0\n"
           "1: [Entry]\n"
           "5: [BranchBool *[0 [st:0]]]\n"
           "[]>*>[1,2]]\n\n"

           "[Block#1\n"
           "7: *[1 [imm 0x4]] = [LoadRoot]\n"
           "9: *[2 [st:0]] = [StoreLocal *[1 [imm 0x4]]]\n"
           "13: [Goto]\n"
           "[0]>*>[6]]\n\n"

           "[Block#6 @[2,8]:9\n"
           "17: [Return *[10 [imm 0x1]]]\n"
           "[1,5]>*>[]]\n\n"

           "[Block#2\n"
           "21: [BranchBool *[3 [st:0]]]\n"
           "[0]>*>[3,4]]\n\n"

           "[Block#3\n"
           "23: *[4 [imm 0x6]] = [LoadRoot]\n"
           "25: *[5 [st:0]] = [StoreLocal *[4 [imm 0x6]]]\n"
           "29: [Goto]\n"
           "[2]>*>[5]]\n\n"

           "[Block#5 @[5,7]:8\n"
           "31: [Goto]\n"
           "[3,4]>*>[6]]\n\n"

           "[Block#4\n"
           "33: *[6 [imm 0x8]] = [LoadRoot]\n"
           "35: *[7 [st:0]] = [StoreLocal *[6 [imm 0x8]]]\n"
           "39: [Goto]\n"
           "[2]>*>[5]]\n\n")

  // While loop
  HIR_TEST("a = 0\nwhile (true) { b = a\na = 2 }\nreturn a",
           "[Block#0\n"
           "1: [Entry]\n"
           "3: *[0 [imm 0x0]] = [LoadRoot]\n"
           "5: *[1 [st:0]] = [StoreLocal *[0 [imm 0x0]]]\n"
           "9: [Goto]\n"
           "[]>*>[1]]\n\n"

           "[Block#1 @[1,5]:6\n"
           "11: *[2 [ctx -2:11]] = [LoadRoot]\n"
           "13: [BranchBool *[2 [ctx -2:11]]]\n"
           "[0,3]>*>[3,2]]\n\n"

           "[Block#3\n"
           "17: *[3 [st:1]] = [StoreLocal *[6 [st:0]]]\n"
           "21: *[4 [imm 0x4]] = [LoadRoot]\n"
           "23: *[5 [st:0]] = [StoreLocal *[4 [imm 0x4]]]\n"
           "27: [Goto]\n"
           "[1]>*>[1]]\n\n"

           "[Block#2\n"
           "31: [Return *[6 [st:0]]]\n"
           "[1]>*>[]]\n\n")

  HIR_TEST("i = 0\nwhile(++i) { }\nreturn i",
           "[Block#0\n"
           "1: [Entry]\n"
           "3: *[0 [imm 0x0]] = [LoadRoot]\n"
           "5: *[1 [st:0]] = [StoreLocal *[0 [imm 0x0]]]\n"
           "9: [Goto]\n"
           "[]>*>[1]]\n\n"

           "[Block#1 @[1,4]:5\n"
           "13: *[2 [imm 0x2]] = [LoadRoot]\n"
           "15: *[3 [st:-1]] = [BinOp *[5 [st:0]] *[2 [imm 0x2]]]\n"
           "17: *[4 [st:0]] = [StoreLocal *[3 [st:-1]]]\n"
           "21: [BranchBool *[3 [st:-1]]]\n"
           "[0,3]>*>[3,2]]\n\n"

           "[Block#3\n"
           "23: [Goto]\n"
           "[1]>*>[1]]\n\n"

           "[Block#2\n"
           "27: [Return *[4 [st:0]]]\n"
           "[1]>*>[]]\n\n")

  // Nested while
  HIR_TEST("while(nil){ while(nil) {} }",
           "[Block#0\n"
           "1: [Entry]\n"
           "3: [Goto]\n"
           "[]>*>[1]]\n\n"

           "[Block#1\n"
           "5: *[0 [imm 0x1]] = [LoadRoot]\n"
           "7: [BranchBool *[0 [imm 0x1]]]\n"
           "[0,5]>*>[3,2]]\n\n"

           "[Block#3\n"
           "9: [Goto]\n"
           "[1]>*>[4]]\n\n"

           "[Block#4\n"
           "11: *[1 [imm 0x1]] = [LoadRoot]\n"
           "13: [BranchBool *[1 [imm 0x1]]]\n"
           "[3,6]>*>[6,5]]\n\n"

           "[Block#6\n"
           "15: [Goto]\n"
           "[4]>*>[4]]\n\n"

           "[Block#5\n"
           "17: [Goto]\n"
           "[4]>*>[1]]\n\n"

           "[Block#2\n"
           "19: [Return *[2 [imm 0x1]]]\n"
           "[1]>*>[]]\n\n")

  // Nested loops and two nested phis
  HIR_TEST("i = 0\nwhile(nil) { while(nil) { i = i + 1 } }\nreturn i",
           "[Block#0\n"
           "1: [Entry]\n"
           "3: *[0 [imm 0x0]] = [LoadRoot]\n"
           "5: *[1 [st:0]] = [StoreLocal *[0 [imm 0x0]]]\n"
           "9: [Goto]\n"
           "[]>*>[1]]\n\n"

           "[Block#1 @[1,7]:8\n"
           "11: *[2 [imm 0x1]] = [LoadRoot]\n"
           "13: [BranchBool *[2 [imm 0x1]]]\n"
           "[0,5]>*>[3,2]]\n\n"

           "[Block#3\n"
           "15: [Goto]\n"
           "[1]>*>[4]]\n\n"

           "[Block#4 @[8,6]:7\n"
           "17: *[3 [imm 0x1]] = [LoadRoot]\n"
           "19: [BranchBool *[3 [imm 0x1]]]\n"
           "[3,6]>*>[6,5]]\n\n"

           "[Block#6\n"
           "23: *[4 [imm 0x2]] = [LoadRoot]\n"
           "25: *[5 [st:-1]] = [BinOp *[7 [st:0]] *[4 [imm 0x2]]]\n"
           "27: *[6 [st:0]] = [StoreLocal *[5 [st:-1]]]\n"
           "31: [Goto]\n"
           "[4]>*>[4]]\n\n"

           "[Block#5\n"
           "33: [Goto]\n"
           "[4]>*>[1]]\n\n"

           "[Block#2\n"
           "37: [Return *[8 [st:0]]]\n"
           "[1]>*>[]]\n\n")

  // Break/continue
  HIR_TEST("while(nil) {\n"
           "if (true) { continue } else { continue }\n"
           "if (true) { break } else { break }\n"
           "}",
           "[Block#0\n"
           "1: [Entry]\n"
           "3: [Goto]\n"
           "[]>*>[3]]\n\n"

           "[Block#3\n"
           "5: [Goto]\n"
           "[0,9]>*>[4]]\n\n"

           "[Block#4\n"
           "7: [Goto]\n"
           "[3,8]>*>[1]]\n\n"

           "[Block#1\n"
           "9: *[0 [imm 0x1]] = [LoadRoot]\n"
           "11: [BranchBool *[0 [imm 0x1]]]\n"
           "[4,13]>*>[7,2]]\n\n"

           "[Block#7\n"
           "13: *[1 [ctx -2:12]] = [LoadRoot]\n"
           "15: [BranchBool *[1 [ctx -2:12]]]\n"
           "[1]>*>[8,9]]\n\n"

           "[Block#8\n"
           "17: [Goto]\n"
           "[7]>*>[4]]\n\n"

           "[Block#9\n"
           "19: [Goto]\n"
           "[7]>*>[3]]\n\n"

           "[Block#2\n"
           "21: [Goto]\n"
           "[1,11]>*>[5]]\n\n"

           "[Block#5\n"
           "23: [Goto]\n"
           "[2,12]>*>[6]]\n\n"

           "[Block#6\n"
           "25: [Return *[3 [imm 0x1]]]\n"
           "[5]>*>[]]\n\n")
TEST_END(hir)
