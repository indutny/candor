#include "test.h"

TEST_START("GC test")
  // Basic test with a context variable
  FUN_TEST("y() {scope x}\nx = 1\nx = 2\n__$gc()\nreturn x", {
    assert(HValue::As<HNumber>(result)->value() == 1);
  })

  // Objects
  FUN_TEST("x = { y : { z : 1 }}\nx.y = { z : 2 }\n__$gc()\nreturn x.y.z", {
    assert(HValue::As<HNumber>(result)->value() == 2);
  })
TEST_END("GC test")
