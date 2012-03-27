#include "test.h"
#include <parser.h>
#include <ast.h>

TEST_START(scope)
  // Global
  SCOPE_TEST("global", "[global @context[-1]:0]")
  SCOPE_TEST("{ global }", "[kBlock [global @context[-1]:0]]")
  SCOPE_TEST("() { global }",
             "[kFunction (anonymous) @[] [global @context[-1]:0]]")

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

  SCOPE_TEST("print = 1\n"
             "((a) {\n"
             "  a((b) {\n"
             "    b()\n"
             "  })\n"
             "})((fn) {\n"
             "  print\n"
             "  fn(() {\n"
             "    print\n"
             "  })\n"
             "})",
             "[kAssign [print @context[0]:0] [1]] "
             "[kCall [kFunction (anonymous) @[[a @stack:0]] "
             "[kCall [a @stack:0] @[[kFunction (anonymous) "
             "@[[b @stack:0]] [kCall [b @stack:0] @[] ]]] ]] "
             "@[[kFunction (anonymous) @[[fn @stack:0]] "
             "[print @context[1]:0] "
             "[kCall [fn @stack:0] "
             "@[[kFunction (anonymous) @[] [print @context[2]:0]]] ]]] ]")
TEST_END(scope)
