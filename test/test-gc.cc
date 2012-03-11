#include "test.h"

TEST_START("GC test")
  FUN_TEST("x=1.0\n"
           "__$gc()\n__$gc()\n__$gc()\n"
           "__$gc()\n__$gc()\n__$gc()\n"
           "return x", {
    assert(result->As<Number>()->Value() == 1);
  })

  FUN_TEST("x = () { return 1 }\n"
           "__$gc()\n"
           "return x()", {
    assert(result->As<Number>()->Value() == 1);
  })

  FUN_TEST("x = { y : { z : 3 } }\n"
           "a() {\n"
           "return (() {\nx.y = 2\n__$gc()\nreturn x.y\n})()\n"
           "}\n"
           "return a()", {
    assert(result->As<Number>()->Value() == 2);
  })

  FUN_TEST("x = [ 1, 2, { y : 1 } ]\n"
           "__$gc()\n"
           "return x[2].y", {
    assert(result->As<Number>()->Value() == 1);
  })

  FUN_TEST("a = { a : { b : 1 } }\n"
           "a = { x: { y: a } }\n"
           "a = { u: { v: a } }\n"
           "__$gc()\n"
           "return a.u.v.x.y.a.b", {
    assert(result->As<Number>()->Value() == 1);
  })

  // Stress test
  FUN_TEST("a = 0\ny = 100\nz=1.0\n"
           "while(--y) {\n"
           "  a = 0\n"
           "  x = 10000\n"
           "  while(--x) {\n"
           "    a = { x: { y: a } }\n"
           "  }\n"
           "}\n"
           "return a.x.y", {
    assert(result->Is<Object>());
  })
TEST_END("GC test")
