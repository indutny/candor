#include "test.h"

TEST_START("functional test")
  // Basics: return + assign
  FUN_TEST("return 1", {
    assert(HNumber::Cast(result)->value() == 1);
  })

  FUN_TEST("a = 32\nreturn a", {
    assert(HNumber::Cast(result)->value() == 32);
  })

  FUN_TEST("a = b = 32\nreturn a", {
    assert(HNumber::Cast(result)->value() == 32);
  })

  FUN_TEST("a = 32\nb = a\nreturn b", {
    assert(HNumber::Cast(result)->value() == 32);
  })

  // Functions
  FUN_TEST("a() {}\nreturn a", {
    assert(HFunction::Cast(result)->addr() != NULL);
  })

  FUN_TEST("a() { return 1 }\nreturn a()", {
    assert(HNumber::Cast(result)->value() == 1);
  })

  // Context slots
  FUN_TEST("b = 13589\na() { scope b }\nreturn b", {
    assert(HNumber::Cast(result)->value() == 13589);
  })

  FUN_TEST("a() { scope a, b\nb = 1234 }\nb = 13589\na()\nreturn b", {
    assert(HNumber::Cast(result)->value() == 1234);
  })

/*
  // Binary ops
  FUN_TEST("return 1 + 2", {
    assert(HNumber::Cast(result)->value() == 3);
  })
*/
TEST_END("functional test")
