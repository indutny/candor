#include "test.h"
#include <parser.h>
#include <ast.h>

TEST_START("parser test")
  // Basic
  PARSER_TEST("a", {})

  // Eq
  PARSER_TEST("a = 1", {})
  PARSER_TEST("a = 1\r\nb=1", {})

  // Binop
  PARSER_TEST("a + b", {})
  PARSER_TEST("a + b + c", {})
  PARSER_TEST("a + b * c", {})
  PARSER_TEST("(a + b) * c", {})

  // Prefix & Postfix
  PARSER_TEST("++a", {})
  PARSER_TEST("!a", {})
  PARSER_TEST("a++", {})

  // Mixed binop + prefix
  PARSER_TEST("++a === b++", {})
  PARSER_TEST("++a !== b++", {})

  // Function delcaration and call
  PARSER_TEST("a()", {})
  PARSER_TEST("a(b,c,d)", {})
  PARSER_TEST("a() {}", {})
  PARSER_TEST("a(b, c, d) { return b }", {})
  PARSER_TEST("(b, c, d) { return b }", {})

  // Member
  PARSER_TEST("a.b.c.d.e", {})
  PARSER_TEST("a[b][c][d][e()]", {})

  // While
  PARSER_TEST("while(true) {}", {})
  PARSER_TEST("while(true) { x = x + 1 }", {})

TEST_END("parser test")
