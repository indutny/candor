#include "test.h"

TEST_START("functional test")
  // Basics: return + assign
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

  // Functions
  FUN_TEST("a() {}\r\nreturn a", {
    assert(result != NULL);
    assert(HFunction::Cast(result)->addr() != NULL);
  })

  FUN_TEST("a() { return 1 }\r\nreturn a()", {
    assert(result != NULL);
    assert(HNumber::Cast(result)->value() == 1);
  })

  // Context slots
  FUN_TEST("b = 13589\r\na() { scope b }\r\nreturn b", {
    assert(result != NULL);
    assert(HNumber::Cast(result)->value() == 13589);
  })

/*
  FUN_TEST("b = 13589\r\na() { scope b\r\nb = 1234 }\r\na()\r\nreturn b", {
    assert(result != NULL);
    assert(HNumber::Cast(result)->value() == 1234);
  })
*/
TEST_END("functional test")
