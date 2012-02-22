#include "test.h"

TEST_START("functional test")
  FUN_TEST("return 1", {
    assert(result != NULL);
    assert(HNumber::Cast(result)->value() == 1);
  })

  FUN_TEST("a = 32\r\nreturn a", {
    assert(result != NULL);
    assert(HNumber::Cast(result)->value() == 32);
  })

  FUN_TEST("a = b = 32\r\nreturn a", {
    assert(result != NULL);
    assert(HNumber::Cast(result)->value() == 32);
  })

  FUN_TEST("a = 32\r\nb = a\r\nreturn b", {
    assert(result != NULL);
    assert(HNumber::Cast(result)->value() == 32);
  })

  FUN_TEST("a() {}\r\nreturn a", {
    assert(result != NULL);
    assert(HFunction::Cast(result)->addr() != NULL);
  })

TEST_END("functional test")
