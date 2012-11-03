#include "test.h"
#include <parser.h>
#include <ast.h>

TEST_START(parser)
  // Basic
  PARSER_TEST("'a'", "[kString a]")
  PARSER_TEST("1", "[1]")
  PARSER_TEST("1.23", "[1.23]")
  PARSER_TEST("-1.23", "[kMinus [1.23]]")
  PARSER_TEST("1 -1.23", "[kSub [1] [1.23]]")
  PARSER_TEST("continue", "[continue]")

  // Comments
  PARSER_TEST("1// comment", "[1]")
  PARSER_TEST("1 // comment", "[1]")
  PARSER_TEST("1// comment\nreturn 1", "[1] [return [1]]")
  PARSER_TEST("return\n// comment", "[return [nil]]")
  PARSER_TEST("return nil\n// nill is default\n", "[return [nil]]")
  PARSER_TEST("1/* comment */ + 1", "[kAdd [1] [1]]")

  // Eq
  PARSER_TEST("a = 1", "[kAssign [a] [1]]")
  PARSER_TEST("a = 1\r\nb=1", "[kAssign [a] [1]] [kAssign [b] [1]]")

  // Binop
  PARSER_TEST("a + b", "[kAdd [a] [b]]")
  PARSER_TEST("a - b - c", "[kSub [a] [kAdd [b] [c]]]")
  PARSER_TEST("a - b - c - d", "[kSub [a] [kAdd [b] [kAdd [c] [d]]]]")
  PARSER_TEST("a + b + c", "[kAdd [a] [kAdd [b] [c]]]")
  PARSER_TEST("a + b * c", "[kAdd [a] [kMul [b] [c]]]")
  PARSER_TEST("a + b * c + d", "[kAdd [a] [kAdd [kMul [b] [c]] [d]]]")
  PARSER_TEST("a * b + c", "[kAdd [kMul [a] [b]] [c]]")
  PARSER_TEST("a * b + c - d * e",
              "[kAdd [kMul [a] [b]] [kSub [c] [kMul [d] [e]]]]")
  PARSER_TEST("(a + b) * c", "[kMul [kAdd [a] [b]] [c]]")
  PARSER_TEST("return 4611686018427387904 + 4611686018427387904 + "
              "4611686018427387904",
              "[return [kAdd [4611686018427387904] "
              "[kAdd [4611686018427387904] [4611686018427387904]]]]")
  PARSER_TEST("a == 1 || b == 2", "[kLOr [kEq [a] [1]] [kEq [b] [2]]]")

  // Prefix & Postfix
  PARSER_TEST("++a", "[kPreInc [a]]")
  PARSER_TEST("++a + 1", "[kAdd [kPreInc [a]] [1]]")
  PARSER_TEST("+a", "[kPlus [a]]")
  PARSER_TEST("!a", "[kNot [a]]")
  PARSER_TEST("a++", "[kPostInc [a]]")
  PARSER_TEST("typeof a", "[kTypeof [a]]")
  PARSER_TEST("sizeof a", "[kSizeof [a]]")
  PARSER_TEST("keysof a", "[kKeysof [a]]")

  // Mixed binop + prefix
  PARSER_TEST("++a === b++", "[kStrictEq [kPreInc [a]] [kPostInc [b]]]")

  // Function delcaration and call
  PARSER_TEST("a()", "[kCall [a] @[] ]")
  PARSER_TEST("a().x", "[kMember [kCall [a] @[] ] [kProperty x]]")
  PARSER_TEST("a()()()", "[kCall [kCall [kCall [a] @[] ] @[] ] @[] ]")
  PARSER_TEST("a(b,c,d)", "[kCall [a] @[[b] [c] [d]] ]")
  PARSER_TEST("a(b,c,d...)", "[kCall [a] @[[b] [c] [kVarArg d [d]]] ]")
  PARSER_TEST("a:b()",
              "[kCall [kMember [a] [kProperty b]] @[[kSelf ]] ]")
  PARSER_TEST("a:b(c, d)",
              "[kCall [kMember [a] [kProperty b]] @[[kSelf ] [c] [d]] ]")
  PARSER_TEST("a():b(c, d)",
              "[kCall [kMember [kCall [a] @[] ] [kProperty b]] "
              "@[[kSelf ] [c] [d]] ]")
  PARSER_TEST("a() {}", "[kFunction [a] @[] [kNop ]]")
  PARSER_TEST("a(b, c, d) { return b }",
              "[kFunction [a] @[[b] [c] [d]] [return [b]]]")
  PARSER_TEST("(b, c, d) { return b }",
              "[kFunction (anonymous) @[[b] [c] [d]] [return [b]]]")
  PARSER_TEST("return (a) {\nreturn a + 2\n}",
              "[return [kFunction (anonymous) @[[a]] "
              "[return [kAdd [a] [2]]]]]")
  PARSER_TEST("return",
              "[return [nil]]")
  PARSER_TEST("return nil",
              "[return [nil]]")

  // Member
  PARSER_TEST("a.b.c.d.e",
              "[kMember [kMember [kMember [kMember [a] [kProperty b]] "
              "[kProperty c]] [kProperty d]] [kProperty e]]")
  PARSER_TEST("a[b][c][d][e()]",
              "[kMember [kMember [kMember [kMember [a] [b]] [c]] [d]] "
              "[kCall [e] @[] ]]")

  // While
  PARSER_TEST("while(true) {}", "[kWhile [true] [kBlock [kNop ]]]")
  PARSER_TEST("while(true) { x = x + 1 }",
              "[kWhile [true] [kBlock [kAssign [x] [kAdd [x] [1]]]]]")
  PARSER_TEST("while(y * y & z) { x = x + 1 }",
              "[kWhile [kBAnd [kMul [y] [y]] [z]] "
              "[kBlock [kAssign [x] [kAdd [x] [1]]]]]")
  PARSER_TEST("while (i >= 0) { x++ }",
              "[kWhile [kGe [i] [0]] [kBlock [kPostInc [x]]]]")

  // If
  PARSER_TEST("if(true) {}", "[if [true] [kBlock [kNop ]]]")
  PARSER_TEST("if(true) {} else {}",
              "[if [true] [kBlock [kNop ]] [kBlock [kNop ]]]")
  PARSER_TEST("if (true) { x }", "[if [true] [kBlock [x]]]")
  PARSER_TEST("if (true) { x } else { y }",
              "[if [true] [kBlock [x]] [kBlock [y]]]")

  // Complex
  PARSER_TEST("p = 0\r\nwhile (true) {\r\nif (p++ > 10) break\ncontinue\n}",
              "[kAssign [p] [0]] [kWhile [true] "
              "[kBlock [if [kGt [kPostInc [p]] [10]] [break]] [continue]]]")

  // Object literal
  PARSER_TEST("a({})", "[kCall [a] @[[kObjectLiteral ]] ]")
  PARSER_TEST("a = { x : 1 }",
              "[kAssign [a] [kObjectLiteral [kProperty x]:[1]]]")
  PARSER_TEST("a = { x : { y: 1 } }",
              "[kAssign [a] [kObjectLiteral [kProperty x]:"
              "[kObjectLiteral [kProperty y]:[1]]]]")
  PARSER_TEST("a = { x : 1, y : 2 }",
              "[kAssign [a] [kObjectLiteral "
              "[kProperty x]:[1] [kProperty y]:[2]]]")
  PARSER_TEST("a = { 1 : 1, 2 : 2 }",
              "[kAssign [a] [kObjectLiteral "
              "[1]:[1] [2]:[2]]]")
  PARSER_TEST("key() {\nreturn 'key'\n}\na = { key: 2 }\nreturn a.key",
              "[kFunction [key] @[] [return [kString key]]] "
              "[kAssign [a] [kObjectLiteral [kProperty key]:[2]]] "
              "[return [kMember [a] [kProperty key]]]")

  // Array literal
  PARSER_TEST("a = [ 1, 2, 3, 4]",
              "[kAssign [a] [kArrayLiteral [1] [2] [3] [4]]]")
  PARSER_TEST("[0][0]",
              "[kMember [kArrayLiteral [0]] [0]]")

  // Nested scopes
  PARSER_TEST("{{{{}}}}", "[kBlock [kBlock [kBlock [kBlock [kNop ]]]]]")

  // Complex names
  PARSER_TEST("__$gc()", "[kCall [__$gc] @[] ]")

  // Wrapping
  PARSER_TEST("return a(\n1,\n2\n)", "[return [kCall [a] @[[1] [2]] ]]")
TEST_END(parser)
