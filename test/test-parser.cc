#include "test.h"
#include <parser.h>
#include <ast.h>

TEST_START("parser test")
  // Basic
  PARSER_TEST("a", "[a]")

  // Eq
  PARSER_TEST("a = 1", "[kAssign [a] [1]]")
  PARSER_TEST("a = 1\r\nb=1", "[kAssign [a] [1]] [kAssign [b] [1]]")

  // Binop
  PARSER_TEST("a + b", "[kAdd [a] [b]]")
  PARSER_TEST("a + b + c", "[kAdd [a] [kAdd [b] [c]]]")
  PARSER_TEST("a + b * c", "[kAdd [a] [kMul [b] [c]]]")
  PARSER_TEST("a * b + c", "[kAdd [kMul [a] [b]] [c]]")
  PARSER_TEST("a * b + c - d * e",
              "[kAdd [kMul [a] [b]] [kSub [c] [kMul [d] [e]]]]")
  PARSER_TEST("(a + b) * c", "[kMul [kAdd [a] [b]] [c]]")

  // Prefix & Postfix
  PARSER_TEST("++a", "[kPreInc [a]]")
  PARSER_TEST("+a", "[kAdd [a]]")
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
  PARSER_TEST("a(b, c, d) { return b }",
              "[kFunction [a] @[[b] [c] [d]] [kReturn [b]]]")
  PARSER_TEST("(b, c, d) { return b }",
              "[kFunction (anonymous) @[[b] [c] [d]] [kReturn [b]]]")

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

  // Block expression
  PARSER_TEST("a({})", "[kCall [a] @[[kBlockExpr [kNop ]]] ]")
  PARSER_TEST("a({ x + 1 })", "[kCall [a] @[[kBlockExpr [kAdd [x] [1]]]] ]")
  PARSER_TEST("a({\n scope x\n x + 1 })",
              "[kCall [a] @[[kBlockExpr [kScopeDecl [x]] "
              "[kAdd [x] [1]]]] ]")

  // Nested scopes
  PARSER_TEST("{{{{}}}}", "[kBlock [kBlock [kBlock [kBlock [kNop ]]]]]")
TEST_END("parser test")
