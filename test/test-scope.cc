#include "test.h"
#include <parser.h>
#include <ast.h>

TEST_START("scope test")
  // Global
  SCOPE_TEST("global", "[global @context[-1]:0]")
  SCOPE_TEST("{ global }", "[kBlock [global @context[-1]:0]]")

  // Function
  SCOPE_TEST("() { a }", "[kFunction (anonymous) @[] [a @stack:0]]")
  SCOPE_TEST("a\n() { a }", "[a @context[0]:0] "
                            "[kFunction (anonymous) @[] [a @context[1]:0]]")
  SCOPE_TEST("() { a\n() { a } }",
             "[kFunction (anonymous) @[] [a @context[0]:0] "
             "[kFunction (anonymous) @[] [a @context[1]:0]]]")

  // While
  SCOPE_TEST("() {while (a) { a ++ } }",
             "[kFunction (anonymous) @[] "
             "[kWhile [a @stack:0] [kBlock [kPostInc [a @stack:0]]]]]")

  // Various
  SCOPE_TEST("a() {}\nc = a()",
             "[kFunction [a @stack:1] @[] [kNop ]] "
             "[kAssign [c @stack:0] [kCall [a @stack:1] @[] ]]")
TEST_END("scope test")
