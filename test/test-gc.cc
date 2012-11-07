#include "test.h"

TEST_START(gc)
  FUN_TEST("x=1.0\n"
           "__$gc()\n__$gc()\n__$gc()\n"
           "__$gc()\n__$gc()\n__$gc()\n"
           "return x", {
    ASSERT(result->As<Number>()->Value() == 1);
  })

  FUN_TEST("x = () { return 1 }\n"
           "__$gc()\n"
           "return x()", {
    ASSERT(result->As<Number>()->Value() == 1);
  })

  FUN_TEST("x = { y : { z : 3 } }\n"
           "a() {\n"
           "return (() {\nx.y = 2\n__$gc()\nreturn x.y\n})()\n"
           "}\n"
           "return a()", {
    ASSERT(result->As<Number>()->Value() == 2);
  })

  FUN_TEST("x = [ 1, 2, { y : 1 } ]\n"
           "__$gc()\n"
           "return x[2].y", {
    ASSERT(result->As<Number>()->Value() == 1);
  })

  FUN_TEST("a = { a : { b : 1 } }\n"
           "a = { x: { y: a } }\n"
           "a = { u: { v: a } }\n"
           "__$gc()\n"
           "return a.u.v.x.y.a.b", {
    ASSERT(result->As<Number>()->Value() == 1);
  })

  // Stress test
  FUN_TEST("a = 0\ny = 30\nz=1.0\n"
           "while(--y) {\n"
           "  a = 0\n"
           "  x = 10000\n"
           "  while(--x) {\n"
           "    a = { x: { y: a } }\n"
           "  }\n"
           "}\n"
           "return a.x.y", {
    ASSERT(result->Is<Object>());
  })
TEST_END(gc)
