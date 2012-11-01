#include "test.h"

TEST_START(numbers)
  // Basics
  FUN_TEST("return 1.2345678", {
    assert(result->As<Number>()->Value() == 1.2345678);
  })

  FUN_TEST("return +1.2345678", {
    assert(result->As<Number>()->Value() == 1.2345678);
  })

  FUN_TEST("return -1.2345678", {
    assert(result->As<Number>()->Value() == -1.2345678);
  })

  FUN_TEST("return 1.5 + 1.5", {
    assert(result->As<Number>()->Value() == 3);
  })

  FUN_TEST("return 3.5 - 1.5", {
    assert(result->As<Number>()->Value() == 2);
  })

  FUN_TEST("return 1.5 * 1.5", {
    assert(result->As<Number>()->Value() == 2.25);
  })

  FUN_TEST("return 7.0 / 2.0", {
    assert(result->As<Number>()->Value() == 3.5);
  })

  // Conversion to unboxed
  FUN_TEST("return 1.5 | 3.5", {
    assert(result->As<Number>()->Value() == 3);
  })

  FUN_TEST("return 2.5 & 3.5", {
    assert(result->As<Number>()->Value() == 2);
  })

  FUN_TEST("return 1.5 ^ 3.5", {
    assert(result->As<Number>()->Value() == 2);
  })

  // Logic
  FUN_TEST("return 1 < 2", {
    assert(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("return 1 == 1", {
    assert(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("return 1.0 < 2.5", {
    assert(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("return 1.0 > 2.5", {
    assert(result->As<Boolean>()->IsFalse());
  })

  FUN_TEST("return 1.0 >= 1.0", {
    assert(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("return 1.0 == 1.0", {
    assert(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("return 1.0 === 1.0", {
    assert(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("return 1 < (0 - 2)", {
    assert(result->As<Boolean>()->IsFalse());
  })

  // Negative (unboxed)
  FUN_TEST("return 0 - 1", {
    assert(result->As<Number>()->Value() == -1);
  })

  FUN_TEST("return 0 - 1 - 2", {
    assert(result->As<Number>()->Value() == -3);
  })

  // Conversion to heap
  FUN_TEST("return 5 + 0.5", {
    assert(result->As<Number>()->Value() == 5.5);
  })

  FUN_TEST("return 0.5 + 5", {
    assert(result->As<Number>()->Value() == 5.5);
  })

  // XXX: This big numbers should be converted to doubles by utils.h
#if CANDOR_ARCH_x64
  // Conversion on overflow
  FUN_TEST("return 2305843009213693952 * 1000000", {
    assert(result->As<Number>()->Value() == 2305843009213693952000000.0);
  })

  FUN_TEST("return 2305843009213693952 + 2305843009213693952 + "
           "2305843009213693952 + 2305843009213693952", {
    assert(result->As<Number>()->Value() == 4.0 * 2305843009213693952.0);
  })

  FUN_TEST("return 0 - 2305843009213693952 - 2305843009213693952 - "
           "2305843009213693952 - 2305843009213693952"
           " - 2305843009213693952", {
    assert(result->As<Number>()->Value() == -5.0 * 2305843009213693952.0);
  })
#endif // CANDOR_ARCH_x64
TEST_END(numbers)
