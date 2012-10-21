#include "test.h"
#include <parser.h>
#include <ast.h>
#include <hir.h>
#include <hir-inl.h>

TEST_START(hir)
  // Simple assignments
  HIR_TEST("a = 1\nb = 1\nreturn a",
           "# Block 0\n"
           "i0 = Entry\n"
           "i2 = Literal\n"
           "i4 = Literal\n"
           "i6 = Return(i2)\n")

  // Multiple blocks and phi
  HIR_TEST("if (a) { a = 2 }\nreturn a",
           "# Block 0\n"
           "i0 = Entry\n"
           "i2 = Nil\n"
           "i4 = If(i2)\n"
           "# succ: 1 2\n"
           "--------\n"
           "# Block 1\n"
           "i6 = Literal\n"
           "i8 = Goto\n"
           "# succ: 3\n"
           "--------\n"
           "# Block 2\n"
           "i12 = Goto\n"
           "# succ: 3\n"
           "--------\n"
           "# Block 3\n"
           "i10 = Phi(i6, i2)\n"
           "i14 = Return(i10)\n")

  HIR_TEST("if (a) { a = 2 } else { a = 3 }\nreturn a",
           "# Block 0\n"
           "i0 = Entry\n"
           "i2 = Nil\n"
           "i4 = If(i2)\n"
           "# succ: 1 2\n"
           "--------\n"
           "# Block 1\n"
           "i6 = Literal\n"
           "i10 = Goto\n"
           "# succ: 3\n"
           "--------\n"
           "# Block 2\n"
           "i8 = Literal\n"
           "i14 = Goto\n"
           "# succ: 3\n"
           "--------\n"
           "# Block 3\n"
           "i12 = Phi(i6, i8)\n"
           "i16 = Return(i12)\n")

  HIR_TEST("a = 1\nif (a) {\n" 
           "  a = 2\n"
           "} else {\n"
           "  if (a) {\n"
           "    if (a) {\n"
           "      a = 3\n"
           "    }\n"
           "  } else {\n"
           "    a = 4\n"
           "  }\n"
           "}\n"
           "return a",
           "# Block 0\n"
           "i0 = Entry\n"
           "i2 = Literal\n"
           "i4 = If(i2)\n"
           "# succ: 1 2\n"
           "--------\n"
           "# Block 1\n"
           "i6 = Literal\n"
           "i32 = Goto\n"
           "# succ: 9\n"
           "--------\n"
           "# Block 2\n"
           "i10 = If(i2)\n"
           "# succ: 3 4\n"
           "--------\n"
           "# Block 3\n"
           "i14 = If(i2)\n"
           "# succ: 5 6\n"
           "--------\n"
           "# Block 4\n"
           "i24 = Literal\n"
           "i30 = Goto\n"
           "# succ: 8\n"
           "--------\n"
           "# Block 5\n"
           "i16 = Literal\n"
           "i18 = Goto\n"
           "# succ: 7\n"
           "--------\n"
           "# Block 6\n"
           "i22 = Goto\n"
           "# succ: 7\n"
           "--------\n"
           "# Block 7\n"
           "i20 = Phi(i16, i2)\n"
           "i26 = Goto\n"
           "# succ: 8\n"
           "--------\n"
           "# Block 8\n"
           "i28 = Phi(i20, i24)\n"
           "i36 = Goto\n"
           "# succ: 9\n"
           "--------\n"
           "# Block 9\n"
           "i34 = Phi(i6, i28)\n"
           "i38 = Return(i34)\n")

  // While loop
  HIR_TEST("a = 0\nwhile (true) { b = a\na = 2 }\nreturn a",
           "# Block 0\n"
           "i0 = Entry\n"
           "i2 = Literal\n"
           "i4 = Goto\n"
           "# succ: 1\n"
           "--------\n"
           "# Block 1\n"
           "i6 = Phi(i2, i18)\n"
           "i10 = Literal\n"
           "i12 = While(i10)\n"
           "# succ: 2 4\n"
           "--------\n"
           "# Block 2\n"
           "i14 = Literal\n"
           "i18 = Literal\n"
           "i20 = Goto\n"
           "# succ: 3\n"
           "--------\n"
           "# Block 3\n"
           "i22 = Goto\n"
           "# succ: 1\n"
           "--------\n"
           "# Block 4\n"
           "i26 = Return(i6)\n")

  /*

  HIR_TEST("i = 0\nwhile(++i) { }\nreturn i",
           "[Block#0\n"
           "0: [Entry]\n"
           "2: *[0 [imm 0x0]] = [LoadRoot]\n"
           "4: *[1 [st:0]] = [StoreLocal *[0 [imm 0x0]]]\n"
           "6: [Goto]\n"
           "[]>*>[1]]\n\n"

           "[Block#1 @[1,5]:2\n"
           "8: [Goto]\n"
           "[0,3]>*>[2]]\n\n"

           "[Block#2\n"
           "10: [Nop *[2 [st:0]]]\n"
           "12: *[3 [imm 0x2]] = [LoadRoot]\n"
           "14: *[4 [st:-1]] = [BinOp *[2 [st:0]] *[3 [imm 0x2]]]\n"
           "16: *[5 [st:0]] = [StoreLocal *[4 [st:-1]]]\n"
           "18: [BranchBool *[5 [st:0]]]\n"
           "[1]>*>[3,4]]\n\n"

           "[Block#3\n"
           "20: [Goto]\n"
           "[2]>*>[1]]\n\n"

           "[Block#4\n"
           "22: [Nop *[5 [st:0]]]\n"
           "24: [Return *[5 [st:0]]]\n"
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
           "6: [Goto]\n"
           "[]>*>[1]]\n\n"

           "[Block#1 @[1,4]:2\n"
           "8: [Goto]\n"
           "[0,8]>*>[2]]\n\n"

           "[Block#2\n"
           "10: *[3 [imm 0x1]] = [LoadRoot]\n"
           "12: [BranchBool *[3 [imm 0x1]]]\n"
           "[1]>*>[3,4]]\n\n"

           "[Block#3\n"
           "14: [Goto]\n"
           "[2]>*>[5]]\n\n"

           "[Block#5 @[2,8]:4\n"
           "16: [Goto]\n"
           "[3,7]>*>[6]]\n\n"

           "[Block#6\n"
           "18: *[5 [imm 0x1]] = [LoadRoot]\n"
           "20: [BranchBool *[5 [imm 0x1]]]\n"
           "[5]>*>[7,8]]\n\n"

           "[Block#7\n"
           "22: [Nop *[4 [st:0]]]\n"
           "24: *[6 [imm 0x2]] = [LoadRoot]\n"
           "26: *[7 [st:-1]] = [BinOp *[4 [st:0]] *[6 [imm 0x2]]]\n"
           "28: *[8 [st:0]] = [StoreLocal *[7 [st:-1]]]\n"
           "30: [Goto]\n"
           "[6]>*>[5]]\n\n"

           "[Block#8\n"
           "32: [Goto]\n"
           "[6]>*>[1]]\n\n"

           "[Block#4\n"
           "34: [Nop *[2 [st:0]]]\n"
           "36: [Return *[2 [st:0]]]\n"
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
           "6: [Goto]\n"
           "[]>*>[1]]\n\n"

           "[Block#1 @[1,17]:2\n"
           "8: [Goto]\n"
           "[0,12]>*>[2]]\n\n"

           "[Block#2 @[2,7]:3\n"
           "10: [Goto]\n"
           "[1,6]>*>[3]]\n\n"

           "[Block#3\n"
           "12: *[4 [imm 0x1]] = [LoadRoot]\n"
           "14: [BranchBool *[4 [imm 0x1]]]\n"
           "[2]>*>[4,5]]\n\n"

           "[Block#4\n"
           "16: [Nop *[3 [st:0]]]\n"
           "18: *[5 [imm 0x4]] = [LoadRoot]\n"
           "20: *[6 [st:-1]] = [BinOp *[3 [st:0]] *[5 [imm 0x4]]]\n"
           "22: *[7 [st:0]] = [StoreLocal *[6 [st:-1]]]\n"
           "24: *[8 [ctx -2:12]] = [LoadRoot]\n"
           "26: [BranchBool *[8 [ctx -2:12]]]\n"
           "[3]>*>[6,7]]\n\n"

           "[Block#6\n"
           "28: [Goto]\n"
           "[4]>*>[2]]\n\n"

           "[Block#7\n"
           "30: [Goto]\n"
           "[4]>*>[8]]\n\n"

           "[Block#8\n"
           "32: [Nop *[7 [st:0]]]\n"
           "34: *[10 [imm 0x6]] = [LoadRoot]\n"
           "36: *[11 [st:-1]] = [BinOp *[7 [st:0]] *[10 [imm 0x6]]]\n"
           "38: *[12 [st:0]] = [StoreLocal *[11 [st:-1]]]\n"
           "40: *[13 [ctx -2:13]] = [LoadRoot]\n"
           "42: [BranchBool *[13 [ctx -2:13]]]\n"
           "[7]>*>[9,10]]\n\n"

           "[Block#9\n"
           "44: [Goto]\n"
           "[8]>*>[11]]\n\n"

           "[Block#10\n"
           "46: [Goto]\n"
           "[8]>*>[12]]\n\n"

           "[Block#12\n"
           "48: [Nop *[12 [st:0]]]\n"
           "50: *[15 [imm 0x8]] = [LoadRoot]\n"
           "52: *[16 [st:-1]] = [BinOp *[12 [st:0]] *[15 [imm 0x8]]]\n"
           "54: *[17 [st:0]] = [StoreLocal *[16 [st:-1]]]\n"
           "56: [Goto]\n"
           "[10]>*>[1]]\n\n"

           "[Block#5\n"
           "58: [Goto]\n"
           "[3]>*>[11]]\n\n"

           "[Block#11 @[3,12]:14\n"
           "60: [Nop *[14 [st:0]]]\n"
           "62: [Return *[14 [st:0]]]\n"
           "[5,9]>*>[]]\n\n")
  */
TEST_END(hir)
