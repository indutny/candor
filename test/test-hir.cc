#include "test.h"
#include <parser.h>
#include <ast.h>
#include <hir.h>
#include <lir.h>

TEST_START(hir)
  // Simple assignments
  HIR_TEST("a = 1\nb = 1",
           "[Block#0\n"
           "0: [Entry]\n"
           "2: *[0 [imm 0x2]] = [LoadRoot]\n"
           "4: *[1 [st:0]] = [StoreLocal *[0 [imm 0x2]]]\n"
           "6: [Nop *[0 [imm 0x2]]]\n"
           "8: *[2 [imm 0x2]] = [LoadRoot]\n"
           "10: *[3 [st:1]] = [StoreLocal *[2 [imm 0x2]]]\n"
           "12: [Nop *[2 [imm 0x2]]]\n"
           "14: [Return *[4 [imm 0x1]]]\n"
           "[]>*>[]]\n\n")
  HIR_TEST("a = 2\na = a",
           "[Block#0\n"
           "0: [Entry]\n"
           "2: *[0 [imm 0x4]] = [LoadRoot]\n"
           "4: *[1 [st:0]] = [StoreLocal *[0 [imm 0x4]]]\n"
           "6: [Nop *[0 [imm 0x4]]]\n"
           "8: [Nop *[1 [st:0]]]\n"
           "10: *[2 [st:0]] = [StoreLocal *[1 [st:0]]]\n"
           "12: [Nop *[1 [st:0]]]\n"
           "14: [Return *[3 [imm 0x1]]]\n"
           "[]>*>[]]\n\n")

  // Multiple blocks and phi
  HIR_TEST("if (a) { a = 2 }\n"
           "// phi should be inserted here\na",
           "[Block#0\n"
           "0: [Entry]\n"
           "2: [Nop *[0 [st:0]]]\n"
           "4: [BranchBool *[0 [st:0]]]\n"
           "[]>*>[1,2]]\n\n"

           "[Block#1\n"
           "6: *[1 [imm 0x4]] = [LoadRoot]\n"
           "8: *[2 [st:0]] = [StoreLocal *[1 [imm 0x4]]]\n"
           "10: [Nop *[1 [imm 0x4]]]\n"
           "12: [Goto]\n"
           "[0]>*>[3]]\n\n"

           "[Block#2\n"
           "14: [Goto]\n"
           "[0]>*>[3]]\n\n"

           "[Block#3 @[2,0]:3\n"
           "16: [Nop *[3 [st:0]]]\n"
           "18: [Return *[4 [imm 0x1]]]\n"
           "[1,2]>*>[]]\n\n")


  HIR_TEST("if (a) { a = 2 } else { a = 3 }\nreturn a",
           "[Block#0\n"
           "0: [Entry]\n"
           "2: [Nop *[0 [st:0]]]\n"
           "4: [BranchBool *[0 [st:0]]]\n"
           "[]>*>[1,2]]\n\n"

           "[Block#1\n"
           "6: *[1 [imm 0x4]] = [LoadRoot]\n"
           "8: *[2 [st:0]] = [StoreLocal *[1 [imm 0x4]]]\n"
           "10: [Nop *[1 [imm 0x4]]]\n"
           "12: [Goto]\n"
           "[0]>*>[3]]\n\n"

           "[Block#2\n"
           "14: *[3 [imm 0x6]] = [LoadRoot]\n"
           "16: *[4 [st:0]] = [StoreLocal *[3 [imm 0x6]]]\n"
           "18: [Nop *[3 [imm 0x6]]]\n"
           "20: [Goto]\n"
           "[0]>*>[3]]\n\n"

           "[Block#3 @[2,4]:5\n"
           "22: [Nop *[5 [st:0]]]\n"
           "24: [Return *[5 [st:0]]]\n"
           "[1,2]>*>[]]\n\n")

  HIR_TEST("if (a) { a = 2 } else { if (a) { a = 3 } else { a = 4 } }\n"
           "return a",
           "[Block#0\n"
           "0: [Entry]\n"
           "2: [Nop *[0 [st:0]]]\n"
           "4: [BranchBool *[0 [st:0]]]\n"
           "[]>*>[1,2]]\n\n"

           "[Block#1\n"
           "6: *[1 [imm 0x4]] = [LoadRoot]\n"
           "8: *[2 [st:0]] = [StoreLocal *[1 [imm 0x4]]]\n"
           "10: [Nop *[1 [imm 0x4]]]\n"
           "12: [Goto]\n"
           "[0]>*>[3]]\n\n"

           "[Block#2\n"
           "14: [Nop *[0 [st:0]]]\n"
           "16: [BranchBool *[0 [st:0]]]\n"
           "[0]>*>[4,5]]\n\n"

           "[Block#4\n"
           "18: *[3 [imm 0x6]] = [LoadRoot]\n"
           "20: *[4 [st:0]] = [StoreLocal *[3 [imm 0x6]]]\n"
           "22: [Nop *[3 [imm 0x6]]]\n"
           "24: [Goto]\n"
           "[2]>*>[6]]\n\n"

           "[Block#5\n"
           "26: *[5 [imm 0x8]] = [LoadRoot]\n"
           "28: *[6 [st:0]] = [StoreLocal *[5 [imm 0x8]]]\n"
           "30: [Nop *[5 [imm 0x8]]]\n"
           "32: [Goto]\n"
           "[2]>*>[6]]\n\n"

           "[Block#6 @[4,6]:7\n"
           "34: [Goto]\n"
           "[4,5]>*>[3]]\n\n"

           "[Block#3 @[2,7]:8\n"
           "36: [Nop *[8 [st:0]]]\n"
           "38: [Return *[8 [st:0]]]\n"
           "[1,6]>*>[]]\n\n")

  // While loop
  HIR_TEST("a = 0\nwhile (true) { b = a\na = 2 }\nreturn a",
           "[Block#0\n"
           "0: [Entry]\n"
           "2: *[0 [imm 0x0]] = [LoadRoot]\n"
           "4: *[1 [st:0]] = [StoreLocal *[0 [imm 0x0]]]\n"
           "6: [Nop *[0 [imm 0x0]]]\n"
           "8: [Goto]\n"
           "[]>*>[1]]\n\n"

           "[Block#1 @[1,6]:2\n"
           "10: [Goto]\n"
           "[0,3]>*>[2]]\n\n"

           "[Block#2\n"
           "12: *[3 [ctx -2:11]] = [LoadRoot]\n"
           "14: [BranchBool *[3 [ctx -2:11]]]\n"
           "[1]>*>[3,4]]\n\n"

           "[Block#3\n"
           "16: [Nop *[2 [st:0]]]\n"
           "18: *[4 [st:1]] = [StoreLocal *[2 [st:0]]]\n"
           "20: [Nop *[2 [st:0]]]\n"
           "22: *[5 [imm 0x4]] = [LoadRoot]\n"
           "24: *[6 [st:0]] = [StoreLocal *[5 [imm 0x4]]]\n"
           "26: [Nop *[5 [imm 0x4]]]\n"
           "28: [Goto]\n"
           "[2]>*>[1]]\n\n"

           "[Block#4\n"
           "30: [Nop *[2 [st:0]]]\n"
           "32: [Return *[2 [st:0]]]\n"
           "[2]>*>[]]\n\n")

  HIR_TEST("i = 0\nwhile(++i) { }\nreturn i",
           "[Block#0\n"
           "0: [Entry]\n"
           "2: *[0 [imm 0x0]] = [LoadRoot]\n"
           "4: *[1 [st:0]] = [StoreLocal *[0 [imm 0x0]]]\n"
           "6: [Nop *[0 [imm 0x0]]]\n"
           "8: [Goto]\n"
           "[]>*>[1]]\n\n"

           "[Block#1 @[1,5]:2\n"
           "10: [Goto]\n"
           "[0,3]>*>[2]]\n\n"

           "[Block#2\n"
           "12: [Nop *[2 [st:0]]]\n"
           "14: *[3 [imm 0x2]] = [LoadRoot]\n"
           "16: *[4 [st:-1]] = [BinOp *[2 [st:0]] *[3 [imm 0x2]]]\n"
           "18: *[5 [st:0]] = [StoreLocal *[4 [st:-1]]]\n"
           "20: [Nop *[4 [st:-1]]]\n"
           "22: [BranchBool *[4 [st:-1]]]\n"
           "[1]>*>[3,4]]\n\n"

           "[Block#3\n"
           "24: [Goto]\n"
           "[2]>*>[1]]\n\n"

           "[Block#4\n"
           "26: [Nop *[5 [st:0]]]\n"
           "28: [Return *[5 [st:0]]]\n"
           "[2]>*>[]]\n\n")

  // Nested while
  HIR_TEST("while(nil){ while(nil) {} }",
           "[Block#0\n"
           "0: [Entry]\n"
           "2: [Goto]\n"
           "[]>*>[1]]\n\n"

           "[Block#1\n"
           "4: [Goto]\n"
           "[0,8]>*>[2]]\n\n"

           "[Block#2\n"
           "6: *[0 [imm 0x1]] = [LoadRoot]\n"
           "8: [BranchBool *[0 [imm 0x1]]]\n"
           "[1]>*>[3,4]]\n\n"

           "[Block#3\n"
           "10: [Goto]\n"
           "[2]>*>[5]]\n\n"

           "[Block#5\n"
           "12: [Goto]\n"
           "[3,7]>*>[6]]\n\n"

           "[Block#6\n"
           "14: *[1 [imm 0x1]] = [LoadRoot]\n"
           "16: [BranchBool *[1 [imm 0x1]]]\n"
           "[5]>*>[7,8]]\n\n"

           "[Block#7\n"
           "18: [Goto]\n"
           "[6]>*>[5]]\n\n"

           "[Block#8\n"
           "20: [Goto]\n"
           "[6]>*>[1]]\n\n"

           "[Block#4\n"
           "22: [Return *[2 [imm 0x1]]]\n"
           "[2]>*>[]]\n\n")

  // Nested loops and two nested phis
  HIR_TEST("i = 0\nwhile(nil) { while(nil) { i = i + 1 } }\nreturn i",
           "[Block#0\n"
           "0: [Entry]\n"
           "2: *[0 [imm 0x0]] = [LoadRoot]\n"
           "4: *[1 [st:0]] = [StoreLocal *[0 [imm 0x0]]]\n"
           "6: [Nop *[0 [imm 0x0]]]\n"
           "8: [Goto]\n"
           "[]>*>[1]]\n\n"

           "[Block#1 @[1,4]:2\n"
           "10: [Goto]\n"
           "[0,8]>*>[2]]\n\n"

           "[Block#2\n"
           "12: *[3 [imm 0x1]] = [LoadRoot]\n"
           "14: [BranchBool *[3 [imm 0x1]]]\n"
           "[1]>*>[3,4]]\n\n"

           "[Block#3\n"
           "16: [Goto]\n"
           "[2]>*>[5]]\n\n"

           "[Block#5 @[2,8]:4\n"
           "18: [Goto]\n"
           "[3,7]>*>[6]]\n\n"

           "[Block#6\n"
           "20: *[5 [imm 0x1]] = [LoadRoot]\n"
           "22: [BranchBool *[5 [imm 0x1]]]\n"
           "[5]>*>[7,8]]\n\n"

           "[Block#7\n"
           "24: [Nop *[4 [st:0]]]\n"
           "26: *[6 [imm 0x2]] = [LoadRoot]\n"
           "28: *[7 [st:-1]] = [BinOp *[4 [st:0]] *[6 [imm 0x2]]]\n"
           "30: *[8 [st:0]] = [StoreLocal *[7 [st:-1]]]\n"
           "32: [Nop *[7 [st:-1]]]\n"
           "34: [Goto]\n"
           "[6]>*>[5]]\n\n"

           "[Block#8\n"
           "36: [Goto]\n"
           "[6]>*>[1]]\n\n"

           "[Block#4\n"
           "38: [Nop *[2 [st:0]]]\n"
           "40: [Return *[2 [st:0]]]\n"
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
           "0: [Entry]\n"
           "2: *[0 [imm 0x2]] = [LoadRoot]\n"
           "4: *[1 [st:0]] = [StoreLocal *[0 [imm 0x2]]]\n"
           "6: [Nop *[0 [imm 0x2]]]\n"
           "8: [Goto]\n"
           "[]>*>[1]]\n\n"

           "[Block#1 @[1,17]:2\n"
           "10: [Goto]\n"
           "[0,12]>*>[2]]\n\n"

           "[Block#2 @[2,7]:3\n"
           "12: [Goto]\n"
           "[1,6]>*>[3]]\n\n"

           "[Block#3\n"
           "14: *[4 [imm 0x1]] = [LoadRoot]\n"
           "16: [BranchBool *[4 [imm 0x1]]]\n"
           "[2]>*>[4,5]]\n\n"

           "[Block#4\n"
           "18: [Nop *[3 [st:0]]]\n"
           "20: *[5 [imm 0x4]] = [LoadRoot]\n"
           "22: *[6 [st:-1]] = [BinOp *[3 [st:0]] *[5 [imm 0x4]]]\n"
           "24: *[7 [st:0]] = [StoreLocal *[6 [st:-1]]]\n"
           "26: [Nop *[6 [st:-1]]]\n"
           "28: *[8 [ctx -2:12]] = [LoadRoot]\n"
           "30: [BranchBool *[8 [ctx -2:12]]]\n"
           "[3]>*>[6,7]]\n\n"

           "[Block#6\n"
           "32: [Goto]\n"
           "[4]>*>[2]]\n\n"

           "[Block#7\n"
           "34: [Goto]\n"
           "[4]>*>[8]]\n\n"

           "[Block#8\n"
           "36: [Nop *[7 [st:0]]]\n"
           "38: *[10 [imm 0x6]] = [LoadRoot]\n"
           "40: *[11 [st:-1]] = [BinOp *[7 [st:0]] *[10 [imm 0x6]]]\n"
           "42: *[12 [st:0]] = [StoreLocal *[11 [st:-1]]]\n"
           "44: [Nop *[11 [st:-1]]]\n"
           "46: *[13 [ctx -2:13]] = [LoadRoot]\n"
           "48: [BranchBool *[13 [ctx -2:13]]]\n"
           "[7]>*>[9,10]]\n\n"

           "[Block#9\n"
           "50: [Goto]\n"
           "[8]>*>[11]]\n\n"

           "[Block#10\n"
           "52: [Goto]\n"
           "[8]>*>[12]]\n\n"

           "[Block#12\n"
           "54: [Nop *[12 [st:0]]]\n"
           "56: *[15 [imm 0x8]] = [LoadRoot]\n"
           "58: *[16 [st:-1]] = [BinOp *[12 [st:0]] *[15 [imm 0x8]]]\n"
           "60: *[17 [st:0]] = [StoreLocal *[16 [st:-1]]]\n"
           "62: [Nop *[16 [st:-1]]]\n"
           "64: [Goto]\n"
           "[10]>*>[1]]\n\n"

           "[Block#5\n"
           "66: [Goto]\n"
           "[3]>*>[11]]\n\n"

           "[Block#11 @[3,12]:14\n"
           "68: [Nop *[14 [st:0]]]\n"
           "70: [Return *[14 [st:0]]]\n"
           "[5,9]>*>[]]\n\n")
TEST_END(hir)
