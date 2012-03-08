#include "test.h"

TEST_START("GC test")
  FUN_TEST("x=1.0\n"
           "__$gc()\n__$gc()\n__$gc()\n"
           "__$gc()\n__$gc()\n__$gc()\n"
           "return 1", {
    assert(result->As<Number>()->Value() == 1);
  })
  FUN_TEST("x = { y : { z : 3 } }\n"
           "a() {\n"
           "return (() {\nscope x\nx.y = 2\n__$gc()\nreturn x.y\n})()\n"
           "}\n"
           "return a()", {
    assert(result->As<Number>()->Value() == 2);
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
           "  scope a\n"
           "  a = 0\n"
           "  x = 10000\n"
           "  while(--x) {\n"
           "    scope a \n"
           "    a = { x: { y: a } }\n"
           "  }\n"
           "}\n"
           "return a.x.y", {
    assert(result->Is<Object>());
  })
TEST_END("GC test")
