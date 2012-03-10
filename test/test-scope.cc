#include "test.h"
#include <parser.h>
#include <ast.h>

TEST_START("scope test")
  // Basic
  SCOPE_TEST("a", "[a @stack:0]")
  SCOPE_TEST("a\n@a", "[a @stack:0] [a @context[-1]:0]")
  SCOPE_TEST("a\n{ @a }", "[a @context[0]:0] [kBlock [a @context[0]:0]]")
  SCOPE_TEST("a.b[c]",
             "[kMember [kMember [a @stack:0] [kProperty b]] [c @stack:1]]")
  SCOPE_TEST("a + b", "[kAdd [a @stack:0] [b @stack:1]]")
  SCOPE_TEST("a\n{ b }", "[a @stack:1] [kBlock [b @stack:0]]")
  SCOPE_TEST("a()", "[kCall [a @stack:0] @[] ]")
  SCOPE_TEST("a\na() { b }",
             "[a @stack:0] [kFunction [a @stack:0] @[] [b @stack:0]]")
  SCOPE_TEST("a\n() { @a }",
             "[a @context[0]:0] "
             "[kFunction (anonymous) @[] [a @context[1]:0]]")
  SCOPE_TEST("a\n() { { @a } }",
              "[a @context[0]:0] "
              "[kFunction (anonymous) @[] "
              "[kBlock [a @context[1]:0]]]")
  SCOPE_TEST("a\n() { @a \n () { @a } \n a }",
             "[a @context[0]:0] "
             "[kFunction (anonymous) @[] [a @context[1]:0] "
             "[kFunction (anonymous) @[] [a @context[1]:0]] "
             "[a @context[0]:0]]")

  // Regressions
  SCOPE_TEST("a() { a = 1\nreturn b() { @a = @a + 1\nreturn @a} }\n",
             "[kFunction [a @stack:0] @[] "
             "[kAssign [a @context[0]:0] [1]] "
             "[kReturn [kFunction [b @stack:0] @[] "
             "[kAssign [a @context[1]:0] [kAdd [a @context[1]:0] [1]]] "
             "[kReturn [a @context[1]:0]]]]]")

  // Global lookup
  SCOPE_TEST("@a", "[a @context[-1]:0]")
  SCOPE_TEST("() {@a}", "[kFunction (anonymous) @[] [a @context[-1]:0]]")

  // Advanced context
  SCOPE_TEST("a\nb\n() {@b}\n() {@a}",
             "[a @context[0]:0] [b @context[0]:1] "
             "[kFunction (anonymous) @[] [b @context[1]:1]] "
             "[kFunction (anonymous) @[] [a @context[1]:0]]")
  SCOPE_TEST("a\nb\n() { () {@b} }\n() {@a}",
             "[a @context[0]:0] [b @context[0]:1] "
             "[kFunction (anonymous) @[] [kFunction (anonymous) @[] "
             "[b @context[2]:1]]] "
             "[kFunction (anonymous) @[] [a @context[1]:0]]")

  // While
  SCOPE_TEST("i = 1\nj = 1\n"
             "while (--i) {@i\n@j = @j + 1\n}\n"
             "return j",
             "[kAssign [i @context[0]:0] [1]] "
             "[kAssign [j @context[0]:1] [1]] "
             "[kWhile [kPreDec [i @context[0]:0]] "
             "[kBlock "
             "[i @context[0]:0] "
             "[kAssign [j @context[0]:1] "
             "[kAdd [j @context[0]:1] [1]]]]] "
             "[kReturn [j @context[0]:1]]")
  SCOPE_TEST("i = 1\nj = 1\na() { @i\n@j\n }\n"
             "while (--i) {@j = @j + 1\n}\n"
             "return j",
             "[kAssign [i @context[0]:0] [1]] "
             "[kAssign [j @context[0]:1] [1]] "
             "[kFunction [a @stack:0] @[] "
             "[i @context[1]:0] [j @context[1]:1]] "
             "[kWhile [kPreDec [i @context[0]:0]] "
             "[kBlock [kAssign [j @context[0]:1] "
             "[kAdd [j @context[0]:1] [1]]]]] "
             "[kReturn [j @context[0]:1]]")

  // Function arguments
  SCOPE_TEST("c = 0\na(a,b)",
             "[kAssign [c @stack:0] [0]] "
             "[kCall [a @stack:1] @[[a @stack:1] [b @stack:2]] ]")
  SCOPE_TEST("c = 0\na(a,b) {}",
             "[kAssign [c @stack:0] [0]] "
             "[kFunction [a @stack:1] @[[a @stack:0] [b @stack:1]] [kNop ]]")
  SCOPE_TEST("(a,b) { return () { @b } }",
             "[kFunction (anonymous) @[[a @stack:0] [b @context[0]:0]] "
             "[kReturn [kFunction (anonymous) @[] "
             "[b @context[1]:0]]]]")

  // Complex
  SCOPE_TEST("((fn) {\n"
             "  @print(\"fn\", fn)\n"
             "  fn2 = fn\n"
             "  return () {\n"
             "    @fn\n"
             "    @print(\"fn2\", @fn2)\n"
             "    @fn2(42)\n"
             "  }\n"
             "})((num) {\n"
             "  @print(\"num\", num)\n"
             "})()",

             "[kCall [kCall "
             "[kFunction (anonymous) @[[fn @context[0]:0]] "
             "[kCall [print @context[-1]:0] @[[kString fn] "
             "[fn @context[0]:0]] ] "
             "[kAssign [fn2 @context[0]:1] [fn @context[0]:0]] "
             "[kReturn [kFunction (anonymous) @[] [fn @context[1]:0] "
             "[kCall [print @context[-1]:0] @[[kString fn2] "
             "[fn2 @context[1]:1]] ] "
             "[kCall [fn2 @context[1]:1] @[[42]] ]]]] "
             "@[[kFunction (anonymous) @[[num @stack:0]] "
             "[kCall [print @context[-1]:0] "
             "@[[kString num] [num @stack:0]] ]]] ] @[] ]")
TEST_END("scope test")
