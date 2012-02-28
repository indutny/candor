#include "test.h"

TEST_START("GC test")
  // Inside function
  FUN_TEST("x = { y : 1 }\n"
           "a() {\nscope x\nx.y = 2\n__$gc()\nreturn x.y\n}\n"
           "return a()", {
    assert(HValue::As<HNumber>(result)->value() == 2);
  })
TEST_END("GC test")
