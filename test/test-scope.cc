#include "test.h"
#include <parser.h>
#include <ast.h>

TEST_START("scope test")
  // Basic
  SCOPE_TEST("a", "[a @stack:0]")
  SCOPE_TEST("a + b", "[kAdd [a @stack:0] [b @stack:1]]")
  SCOPE_TEST("a\r\n{ b }", "[a @stack:1] [kBlock [b @stack:0]]")
  SCOPE_TEST("a\r\na() { b }",
             "[a @stack:0] [kFunction [a @stack:0] @[] [b @stack:0]]")
  SCOPE_TEST("a\r\n() { scope a\r\n a }",
             "[a @context[0]:0] "
             "[kFunction (anonymous) @[] [kScopeDecl [a]] [a @context[1]:0]]")
  SCOPE_TEST("a\r\n() { { scope a\r\n a } }",
              "[a @context[0]:0] "
              "[kFunction (anonymous) @[] "
              "[kBlock [kScopeDecl [a]] [a @context[1]:0]]]")
  SCOPE_TEST("a\r\n() { scope a \n () { scope a\r\n a } \r\n a }",
             "[a @context[0]:0] "
             "[kFunction (anonymous) @[] [kScopeDecl [a]] "
             "[kFunction (anonymous) @[] [kScopeDecl [a]] "
             "[a @context[2]:0]] [a @context[1]:0]]")

  // Global lookup
  SCOPE_TEST("scope a\r\na", "[kScopeDecl [a]] [a @context[-1]:0]")
  SCOPE_TEST("() {scope a\r\na}",
             "[kFunction (anonymous) @[] [kScopeDecl [a]] [a @context[-1]:0]]")
TEST_END("scope test")
