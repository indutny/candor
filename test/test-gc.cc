#include "test.h"

TEST_START("GC test")
  // Basic test with a context variable
  FUN_TEST("y() {scope x}\nx = 1\nx = 2\n__$gc()\nreturn x", {
  })
TEST_END("GC test")
