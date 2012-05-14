#include "test.h"
#include <parser.h>
#include <ast.h>
#include <hir.h>
#include <hir-inl.h>
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

           "[Block#2\n"
           "15: [Goto]\n"
           "[0]>*>[3]]\n\n"

           "[Block#3 @[2,0]:3\n"
           "19: [Return *[4 [imm 0x1]]]\n"
           "[1,2]>*>[]]\n\n")


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

           "[Block#2\n"
           "15: *[3 [imm 0x6]] = [LoadRoot]\n"
           "17: *[4 [st:0]] = [StoreLocal *[3 [imm 0x6]]]\n"
           "21: [Goto]\n"
           "[0]>*>[3]]\n\n"

           "[Block#3 @[2,4]:5\n"
           "25: [Return *[6 [imm 0x1]]]\n"
           "[1,2]>*>[]]\n\n")

  HIR_TEST("if (a) { a = 2 } else { if (a) { a = 3 } else { a = 4 } }\n"
           "return a",
           "[Block#0\n"
           "1: [Entry]\n"
           "5: [BranchBool *[0 [st:0]]]\n"
           "[]>*>[1,2]]\n\n"

           "[Block#1\n"
           "7: *[1 [imm 0x4]] = [LoadRoot]\n"
           "9: *[2 [st:0]] = [StoreLocal *[1 [imm 0x4]]]\n"
           "13: [Goto]\n"
           "[0]>*>[3]]\n\n"

           "[Block#2\n"
           "17: [BranchBool *[0 [st:0]]]\n"
           "[0]>*>[4,5]]\n\n"

           "[Block#4\n"
           "19: *[3 [imm 0x6]] = [LoadRoot]\n"
           "21: *[4 [st:0]] = [StoreLocal *[3 [imm 0x6]]]\n"
           "25: [Goto]\n"
           "[2]>*>[6]]\n\n"

           "[Block#5\n"
           "27: *[5 [imm 0x8]] = [LoadRoot]\n"
           "29: *[6 [st:0]] = [StoreLocal *[5 [imm 0x8]]]\n"
           "33: [Goto]\n"
           "[2]>*>[6]]\n\n"

           "[Block#6 @[4,6]:7\n"
           "35: [Goto]\n"
           "[4,5]>*>[3]]\n\n"

           "[Block#3 @[2,7]:8\n"
           "39: [Return *[8 [st:0]]]\n"
           "[1,6]>*>[]]\n\n")

  // While loop
  HIR_TEST("a = 0\nwhile (true) { b = a\na = 2 }\nreturn a",
           "[Block#0\n"
           "1: [Entry]\n"
           "3: *[0 [imm 0x0]] = [LoadRoot]\n"
           "5: *[1 [st:0]] = [StoreLocal *[0 [imm 0x0]]]\n"
           "9: [Goto]\n"
           "[]>*>[1]]\n\n"

           "[Block#1 @[1,6]:2\n"
           "11: [Goto]\n"
           "[0,3]>*>[2]]\n\n"

           "[Block#2\n"
           "13: *[3 [ctx -2:11]] = [LoadRoot]\n"
           "15: [BranchBool *[3 [ctx -2:11]]]\n"
           "[1]>*>[3,4]]\n\n"

           "[Block#3\n"
           "19: *[4 [st:1]] = [StoreLocal *[2 [st:0]]]\n"
           "23: *[5 [imm 0x4]] = [LoadRoot]\n"
           "25: *[6 [st:0]] = [StoreLocal *[5 [imm 0x4]]]\n"
           "29: [Goto]\n"
           "[2]>*>[1]]\n\n"

           "[Block#4\n"
           "33: [Return *[2 [st:0]]]\n"
           "[2]>*>[]]\n\n")

  HIR_TEST("i = 0\nwhile(++i) { }\nreturn i",
           "[Block#0\n"
           "1: [Entry]\n"
           "3: *[0 [imm 0x0]] = [LoadRoot]\n"
           "5: *[1 [st:0]] = [StoreLocal *[0 [imm 0x0]]]\n"
           "9: [Goto]\n"
           "[]>*>[1]]\n\n"

           "[Block#1 @[1,5]:2\n"
           "11: [Goto]\n"
           "[0,3]>*>[2]]\n\n"

           "[Block#2\n"
           "15: *[3 [imm 0x2]] = [LoadRoot]\n"
           "17: *[4 [st:-1]] = [BinOp *[2 [st:0]] *[3 [imm 0x2]]]\n"
           "19: *[5 [st:0]] = [StoreLocal *[4 [st:-1]]]\n"
           "23: [BranchBool *[4 [st:-1]]]\n"
           "[1]>*>[3,4]]\n\n"

           "[Block#3\n"
           "25: [Goto]\n"
           "[2]>*>[1]]\n\n"

           "[Block#4\n"
           "29: [Return *[5 [st:0]]]\n"
           "[2]>*>[]]\n\n")

  // Nested while
  HIR_TEST("while(nil){ while(nil) {} }",
           "[Block#0\n"
           "1: [Entry]\n"
           "3: [Goto]\n"
           "[]>*>[1]]\n\n"

           "[Block#1\n"
           "5: [Goto]\n"
           "[0,8]>*>[2]]\n\n"

           "[Block#2\n"
           "7: *[0 [imm 0x1]] = [LoadRoot]\n"
           "9: [BranchBool *[0 [imm 0x1]]]\n"
           "[1]>*>[3,4]]\n\n"

           "[Block#3\n"
           "11: [Goto]\n"
           "[2]>*>[5]]\n\n"

           "[Block#5\n"
           "13: [Goto]\n"
           "[3,7]>*>[6]]\n\n"

           "[Block#6\n"
           "15: *[1 [imm 0x1]] = [LoadRoot]\n"
           "17: [BranchBool *[1 [imm 0x1]]]\n"
           "[5]>*>[7,8]]\n\n"

           "[Block#7\n"
           "19: [Goto]\n"
           "[6]>*>[5]]\n\n"

           "[Block#8\n"
           "21: [Goto]\n"
           "[6]>*>[1]]\n\n"

           "[Block#4\n"
           "23: [Return *[2 [imm 0x1]]]\n"
           "[2]>*>[]]\n\n")

  // Nested loops and two nested phis
  HIR_TEST("i = 0\nwhile(nil) { while(nil) { i = i + 1 } }\nreturn i",
           "[Block#0\n"
           "1: [Entry]\n"
           "3: *[0 [imm 0x0]] = [LoadRoot]\n"
           "5: *[1 [st:0]] = [StoreLocal *[0 [imm 0x0]]]\n"
           "9: [Goto]\n"
           "[]>*>[1]]\n\n"

           "[Block#1 @[1,4]:2\n"
           "11: [Goto]\n"
           "[0,8]>*>[2]]\n\n"

           "[Block#2\n"
           "13: *[3 [imm 0x1]] = [LoadRoot]\n"
           "15: [BranchBool *[3 [imm 0x1]]]\n"
           "[1]>*>[3,4]]\n\n"

           "[Block#3\n"
           "17: [Goto]\n"
           "[2]>*>[5]]\n\n"

           "[Block#5 @[2,8]:4\n"
           "19: [Goto]\n"
           "[3,7]>*>[6]]\n\n"

           "[Block#6\n"
           "21: *[5 [imm 0x1]] = [LoadRoot]\n"
           "23: [BranchBool *[5 [imm 0x1]]]\n"
           "[5]>*>[7,8]]\n\n"

           "[Block#7\n"
           "27: *[6 [imm 0x2]] = [LoadRoot]\n"
           "29: *[7 [st:-1]] = [BinOp *[4 [st:0]] *[6 [imm 0x2]]]\n"
           "31: *[8 [st:0]] = [StoreLocal *[7 [st:-1]]]\n"
           "35: [Goto]\n"
           "[6]>*>[5]]\n\n"

           "[Block#8\n"
           "37: [Goto]\n"
           "[6]>*>[1]]\n\n"

           "[Block#4\n"
           "41: [Return *[2 [st:0]]]\n"
           "[2]>*>[]]\n\n")

  // Break/continue
  HIR_TEST("a = 1\n"
           "while(nil) {\n"
           "a = a + 2\n"
           "if (true) { continue }\n"
           "a = a + 3\n"
           "if (false) { break }\n"
           "a = a + 4\n"
           "}\n"
           "return a",

           "[Block#0\n"
           "1: [Entry]\n"
           "3: *[0 [imm 0x2]] = [LoadRoot]\n"
           "5: *[1 [st:0]] = [StoreLocal *[0 [imm 0x2]]]\n"
           "9: [Goto]\n"
           "[]>*>[1]]\n\n"

           "[Block#1 @[1,17]:2\n"
           "11: [Goto]\n"
           "[0,12]>*>[2]]\n\n"

           "[Block#2 @[2,7]:3\n"
           "13: [Goto]\n"
           "[1,6]>*>[3]]\n\n"

           "[Block#3\n"
           "15: *[4 [imm 0x1]] = [LoadRoot]\n"
           "17: [BranchBool *[4 [imm 0x1]]]\n"
           "[2]>*>[4,5]]\n\n"

           "[Block#4\n"
           "21: *[5 [imm 0x4]] = [LoadRoot]\n"
           "23: *[6 [st:-1]] = [BinOp *[3 [st:0]] *[5 [imm 0x4]]]\n"
           "25: *[7 [st:0]] = [StoreLocal *[6 [st:-1]]]\n"
           "29: *[8 [ctx -2:12]] = [LoadRoot]\n"
           "31: [BranchBool *[8 [ctx -2:12]]]\n"
           "[3]>*>[6,7]]\n\n"

           "[Block#6\n"
           "33: [Goto]\n"
           "[4]>*>[2]]\n\n"

           "[Block#7\n"
           "35: [Goto]\n"
           "[4]>*>[8]]\n\n"

           "[Block#8\n"
           "39: *[10 [imm 0x6]] = [LoadRoot]\n"
           "41: *[11 [st:-1]] = [BinOp *[7 [st:0]] *[10 [imm 0x6]]]\n"
           "43: *[12 [st:0]] = [StoreLocal *[11 [st:-1]]]\n"
           "47: *[13 [ctx -2:13]] = [LoadRoot]\n"
           "49: [BranchBool *[13 [ctx -2:13]]]\n"
           "[7]>*>[9,10]]\n\n"

           "[Block#9\n"
           "51: [Goto]\n"
           "[8]>*>[11]]\n\n"

           "[Block#10\n"
           "53: [Goto]\n"
           "[8]>*>[12]]\n\n"

           "[Block#12\n"
           "57: *[15 [imm 0x8]] = [LoadRoot]\n"
           "59: *[16 [st:-1]] = [BinOp *[12 [st:0]] *[15 [imm 0x8]]]\n"
           "61: *[17 [st:0]] = [StoreLocal *[16 [st:-1]]]\n"
           "65: [Goto]\n"
           "[10]>*>[1]]\n\n"

           "[Block#5\n"
           "67: [Goto]\n"
           "[3]>*>[11]]\n\n"

           "[Block#11 @[3,12]:14\n"
           "71: [Return *[14 [st:0]]]\n"
           "[5,9]>*>[]]\n\n")
TEST_END(hir)
