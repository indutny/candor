#include "test.h"

TEST_START("numbers test")
  // Basics
  FUN_TEST("return 1.2345678", {
    assert(HValue::As<HNumber>(result)->value() == 1.2345678);
  })

  FUN_TEST("return 1.5 + 1.5", {
    assert(HValue::As<HNumber>(result)->value() == 3);
  })

  FUN_TEST("return 3.5 - 1.5", {
    assert(HValue::As<HNumber>(result)->value() == 2);
  })

  FUN_TEST("return 1.5 * 1.5", {
    assert(HValue::As<HNumber>(result)->value() == 2.25);
  })

  FUN_TEST("return 7.0 / 2.0", {
    assert(HValue::As<HNumber>(result)->value() == 3.5);
  })

  // Negative (unboxed)
  FUN_TEST("return 0 - 1", {
    assert(HValue::As<HNumber>(result)->value() == -1);
  })

  // Conversion to heap
  FUN_TEST("return 5 + 0.5", {
    assert(HValue::As<HNumber>(result)->value() == 5.5);
  })

  FUN_TEST("return 0.5 + 5", {
    assert(HValue::As<HNumber>(result)->value() == 5.5);
  })

  // Conversion on overflow
  FUN_TEST("return 4611686018427387904 * 1000000", {
    assert(HValue::As<HNumber>(result)->value() == 4611686018427387904000000.0);
  })
TEST_END("numbers test")
