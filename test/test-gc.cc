#include "test.h"

TEST_START("GC test")
  // Basic test with a context variable
  FUN_TEST("y() {scope x}\nx = 1\nx = 2\n__$gc()\nreturn x", {
    assert(HValue::As<HNumber>(result)->value() == 2);
  })

  // Objects
  FUN_TEST("x = { y : 1 }\nx.y = 2\n__$gc()\nreturn x.y", {
    assert(HValue::As<HNumber>(result)->value() == 2);
  })

  FUN_TEST("x = { y : { z : 1 }}\nx.y = { z : 2 }\n__$gc()\nreturn x.y.z", {
    assert(HValue::As<HNumber>(result)->value() == 2);
  })

  // Inside function
  FUN_TEST("x = { y : 1 }\n"
           "a() {\nscope x\nx.y = 2\n__$gc()\nreturn x.y\n}\n"
           "return a()", {
    assert(HValue::As<HNumber>(result)->value() == 2);
  })
TEST_END("GC test")
