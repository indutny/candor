#include "test.h"

TEST_START("numbers test")
  FUN_TEST("return 1.2345678", {
    assert(HValue::As<HNumber>(result)->value() == 1.2345678);
  })

  FUN_TEST("return 1.5 + 1.5", {
    assert(HValue::As<HNumber>(result)->value() == 3);
  })
TEST_END("numbers test")
