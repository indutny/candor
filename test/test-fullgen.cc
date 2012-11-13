#include "test.h"
#include <parser.h>
#include <ast.h>
#include <fullgen.h>
#include <fullgen-inl.h>

TEST_START(fullgen)
  FULLGEN_TEST("return 1 + 2 + 3\n",
               "0 Label\n"
               "2 Entry\n"
               "4 s[1] = Literal\n"
               "6 s[3] = Literal\n"
               "8 s[4] = Literal\n"
               "10 s[2] = BinOp(s[3], s[4])\n"
               "12 s[0] = BinOp(s[1], s[2])\n"
               "14 Return(s[0])\n"
               "16 s[0] = Nil\n"
               "18 Return(s[0])\n")
  FULLGEN_TEST("i = 0\nreturn i\n",
               "0 Label\n"
               "2 Entry\n"
               "4 s[1] = Literal\n"
               "6 Store(s[0], s[1])\n"
               "8 s[1] = Load(s[0])\n"
               "10 Return(s[1])\n"
               "12 s[1] = Nil\n"
               "14 Return(s[1])\n")
  FULLGEN_TEST("a = { y: 1 }\na.x = 0\ndelete a.y\nreturn a.x\n",
               "0 Label\n"
               "2 Entry\n"
               "4 s[2] = AllocateObject\n"
               "6 s[3] = Literal\n"
               "8 s[4] = Literal\n"
               "10 StoreProperty(s[2], s[4], s[3])\n"
               "12 s[1] = Chi(s[2])\n"
               "14 Store(s[0], s[1])\n"
               "16 s[1] = Literal\n"
               "18 s[2] = Literal\n"
               "20 s[3] = Load(s[0])\n"
               "22 StoreProperty(s[3], s[2], s[1])\n"
               "24 s[1] = Literal\n"
               "26 s[2] = Load(s[0])\n"
               "28 DeleteProperty(s[2], s[1])\n"
               "30 s[2] = Literal\n"
               "32 s[3] = Load(s[0])\n"
               "34 s[1] = LoadProperty(s[3], s[2])\n"
               "36 Return(s[1])\n"
               "38 s[1] = Nil\n"
               "40 Return(s[1])\n")
  FULLGEN_TEST("return a++\n",
               "0 Label\n"
               "2 Entry\n"
               "4 s[3] = Load(s[0])\n"
               "6 s[4] = Literal\n"
               "8 s[2] = BinOp(s[3], s[4])\n"
               "10 Store(s[0], s[2])\n"
               "12 s[1] = Chi(s[3])\n"
               "14 Return(s[1])\n"
               "16 s[1] = Nil\n"
               "18 Return(s[1])\n")
  FULLGEN_TEST("return ++a\n",
               "0 Label\n"
               "2 Entry\n"
               "4 s[3] = Load(s[0])\n"
               "6 s[4] = Literal\n"
               "8 s[2] = BinOp(s[3], s[4])\n"
               "10 Store(s[0], s[2])\n"
               "12 s[1] = Chi(s[2])\n"
               "14 Return(s[1])\n"
               "16 s[1] = Nil\n"
               "18 Return(s[1])\n")
  FULLGEN_TEST("return ++a.b\n",
               "0 Label\n"
               "2 Entry\n"
               "4 s[5] = Literal\n"
               "6 s[6] = Load(s[0])\n"
               "8 s[3] = LoadProperty(s[6], s[5])\n"
               "10 s[4] = Literal\n"
               "12 s[2] = BinOp(s[3], s[4])\n"
               "14 StoreProperty(s[6], s[5], s[2])\n"
               "16 s[1] = Chi(s[2])\n"
               "18 Return(s[1])\n"
               "20 s[1] = Nil\n"
               "22 Return(s[1])\n")
  FULLGEN_TEST("return +a\n",
               "0 Label\n"
               "2 Entry\n"
               "4 s[2] = Literal\n"
               "6 s[3] = Load(s[0])\n"
               "8 s[1] = BinOp(s[2], s[3])\n"
               "10 Return(s[1])\n"
               "12 s[1] = Nil\n"
               "14 Return(s[1])\n")
  FULLGEN_TEST("return clone a\n",
               "0 Label\n"
               "2 Entry\n"
               "4 s[2] = Load(s[0])\n"
               "6 s[1] = Clone(s[2])\n"
               "8 Return(s[1])\n"
               "10 s[1] = Nil\n"
               "12 Return(s[1])\n")
  FULLGEN_TEST("if (1) {\na = 1\n} else {\na = 2\n}\nreturn a",
               "0 Label\n"
               "2 Entry\n"
               "4 s[1] = Literal\n"
               "6 If (s[1]) => 8 Else 16\n"
               "8 Label\n"
               "10 s[2] = Literal\n"
               "12 Store(s[0], s[2])\n"
               "14 Goto => 22\n"
               "16 Label\n"
               "18 s[2] = Literal\n"
               "20 Store(s[0], s[2])\n"
               "22 Label\n"
               "24 s[1] = Load(s[0])\n"
               "26 Return(s[1])\n"
               "28 s[1] = Nil\n"
               "30 Return(s[1])\n")
  FULLGEN_TEST("while (1) {\nwhile(2) { break }\nbreak\n}",
               "0 Label\n"
               "2 Entry\n"
               "4 s[0] = Literal\n"
               "6 Label\n"
               "8 If (s[0]) => 10 Else 30\n"
               "10 Label\n"
               "12 s[1] = Literal\n"
               "14 Label\n"
               "16 If (s[1]) => 18 Else 24\n"
               "18 Label\n"
               "20 Break => 24\n"
               "22 Goto => 14\n"
               "24 Label\n"
               "26 Break => 30\n"
               "28 Goto => 6\n"
               "30 Label\n"
               "32 s[0] = Nil\n"
               "34 Return(s[0])\n")
TEST_END(fullgen)
