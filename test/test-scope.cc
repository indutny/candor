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
  SCOPE_TEST("a\n(a) { a }", "[a @stack:0] "
                            "[kFunction (anonymous) @[[a @stack:0]] [a @stack:0]]")
  SCOPE_TEST("a\n(a, b) { a\nb }",
             "[a @stack:0] "
             "[kFunction (anonymous) @[[a @stack:0] [b @stack:1]] "
             "[a @stack:0] [b @stack:1]]")
  SCOPE_TEST("() { a\n() { a } }",
             "[kFunction (anonymous) @[] [a @context[0]:0] "
             "[kFunction (anonymous) @[] [a @context[1]:0]]]")
  SCOPE_TEST("a() { }\nb(() { a })",
             "[kAssign [a @context[0]:0] [kFunction (anonymous) @[] [kNop ]]] "
             "[kCall [b @stack:0] "
             "@[[kFunction (anonymous) @[] [a @context[1]:0]]]"
             " ]")
  SCOPE_TEST("a() { b = 1234 }\nb = 13589\na()\nreturn b",
             "[kAssign [a @stack:0] "
             "[kFunction (anonymous) @[] [kAssign [b @context[1]:0] [1234]]]] "
             "[kAssign [b @context[0]:0] [13589]] "
             "[kCall [a @stack:0] @[] ] [return [b @context[0]:0]]")

  // While
  SCOPE_TEST("() {while (a) { a ++ } }",
             "[kFunction (anonymous) @[] "
             "[kWhile [a @stack:0] [kBlock [kPostInc [a @stack:0]]]]]")

  // Various
  SCOPE_TEST("a() {}\nc = a()",
             "[kAssign [a @stack:1] [kFunction (anonymous) @[] [kNop ]]] "
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
