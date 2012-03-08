#include "test.h"
#include <parser.h>
#include <ast.h>

TEST_START("parser test")
  // Basic
  PARSER_TEST("'a'", "[kString a]")
  PARSER_TEST("1", "[1]")
  PARSER_TEST("1.23", "[1.23]")
  PARSER_TEST("-1.23", "[kMinus [1.23]]")
  PARSER_TEST("1 -1.23", "[kSub [1] [1.23]]")

  // Comments
  PARSER_TEST("1// comment", "[1]")
  PARSER_TEST("1 // comment", "[1]")
  PARSER_TEST("1// comment\nreturn 1", "[1] [kReturn [1]]")
  PARSER_TEST("return\n// comment", "[kReturn [nil]]")
  PARSER_TEST("return nil\n// nill is default\n", "[kReturn [nil]]")
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
  PARSER_TEST("a * b + c", "[kAdd [kMul [a] [b]] [c]]")
  PARSER_TEST("a * b + c - d * e",
              "[kAdd [kMul [a] [b]] [kSub [c] [kMul [d] [e]]]]")
  PARSER_TEST("(a + b) * c", "[kMul [kAdd [a] [b]] [c]]")
  PARSER_TEST("return 4611686018427387904 + 4611686018427387904 + "
              "4611686018427387904",
              "[kReturn [kAdd [4611686018427387904] "
              "[kAdd [4611686018427387904] [4611686018427387904]]]]")

  // Prefix & Postfix
  PARSER_TEST("++a", "[kPreInc [a]]")
  PARSER_TEST("+a", "[kPlus [a]]")
  PARSER_TEST("!a", "[kNot [a]]")
  PARSER_TEST("a++", "[kPostInc [a]]")

  // Mixed binop + prefix
  PARSER_TEST("++a === b++", "[kPreInc [kStrictEq [a] [kPostInc [b]]]]")
  PARSER_TEST("++a !== b++", "[kPreInc [kStrictNe [a] [kPostInc [b]]]]")

  // Function delcaration and call
  PARSER_TEST("a()", "[kCall [a] @[] ]")
  PARSER_TEST("a()()()", "[kCall [kCall [kCall [a] @[] ] @[] ] @[] ]")
  PARSER_TEST("a(b,c,d)", "[kCall [a] @[[b] [c] [d]] ]")
  PARSER_TEST("a() {}", "[kFunction [a] @[] [kNop ]]")
  PARSER_TEST("a() {scope x}", "[kFunction [a] @[] [kScopeDecl [x]]]")
  PARSER_TEST("a() {\nscope x\n}", "[kFunction [a] @[] [kScopeDecl [x]]]")
  PARSER_TEST("a(b, c, d) { return b }",
              "[kFunction [a] @[[b] [c] [d]] [kReturn [b]]]")
  PARSER_TEST("(b, c, d) { return b }",
              "[kFunction (anonymous) @[[b] [c] [d]] [kReturn [b]]]")
  PARSER_TEST("return (a) {\nreturn a + 2\n}",
              "[kReturn [kFunction (anonymous) @[[a]] "
              "[kReturn [kAdd [a] [2]]]]]")
  PARSER_TEST("return",
              "[kReturn [nil]]")
  PARSER_TEST("return nil",
              "[kReturn [nil]]")

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
  PARSER_TEST("if(true) {}", "[kIf [true] [kBlock [kNop ]]]")
  PARSER_TEST("if(true) {} else {}",
              "[kIf [true] [kBlock [kNop ]] [kBlock [kNop ]]]")
  PARSER_TEST("if (true) { x }", "[kIf [true] [kBlock [x]]]")
  PARSER_TEST("if (true) { x } else { y }",
              "[kIf [true] [kBlock [x]] [kBlock [y]]]")

  // Complex
  PARSER_TEST("p = 0\r\nwhile (true) {\r\nif (p++ > 10) break\n}",
              "[kAssign [p] [0]] [kWhile [true] "
              "[kBlock [kIf [kGt [kPostInc [p]] [10]] [kBreak ]]]]")

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
              "[kProperty 1]:[1] [kProperty 2]:[2]]]")
  PARSER_TEST("key() {\nreturn 'key'\n}\na = { key: 2 }\nreturn a.key",
              "[kFunction [key] @[] [kReturn [kString key]]] "
              "[kAssign [a] [kObjectLiteral [kProperty key]:[2]]] "
              "[kReturn [kMember [a] [kProperty key]]]")

  // Array literal
  PARSER_TEST("a = [ 1, 2, 3, 4]",
              "[kAssign [a] [kArrayLiteral [1] [2] [3] [4]]]")

  // Nested scopes
  PARSER_TEST("{{{{}}}}", "[kBlock [kBlock [kBlock [kBlock [kNop ]]]]]")

  // Complex names
  PARSER_TEST("__$gc()", "[kCall [__$gc] @[] ]")
TEST_END("parser test")
