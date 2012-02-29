#include "test.h"

TEST_START("numbers test")
  FUN_TEST("return 1.2345678", {
    assert(HValue::As<HNumber>(result)->value() == 1.2345678);
  })
TEST_END("numbers test")
