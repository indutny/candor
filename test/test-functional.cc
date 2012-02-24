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

  FUN_TEST("a(b) { return b }\nreturn a(3) + a(4)", {
    assert(HNumber::Cast(result)->value() == 7);
  })

  FUN_TEST("a(b) { return b }\nreturn a()", {
    assert(result == NULL);
  })

  FUN_TEST("b() {\nreturn 1\n}\na(c) {\nreturn c()\n}\nreturn a(b)", {
    assert(HNumber::Cast(result)->value() == 1);
  })

  FUN_TEST("a(c) {\nreturn c()\n}\nreturn a(() {\nreturn 1\n})", {
    assert(HNumber::Cast(result)->value() == 1);
  })

  // Context slots
  FUN_TEST("b = 13589\na() { scope b }\nreturn b", {
    assert(HNumber::Cast(result)->value() == 13589);
  })

  FUN_TEST("a() { scope a, b\nb = 1234 }\nb = 13589\na()\nreturn b", {
    assert(HNumber::Cast(result)->value() == 1234);
  })

  FUN_TEST("a() { a = 1\nreturn b() { scope a\nreturn a} }\nreturn a()()", {
    assert(HNumber::Cast(result)->value() == 1);
  });

  // Binary ops
  FUN_TEST("return 1 + 2", {
    assert(HNumber::Cast(result)->value() == 3);
  })

  FUN_TEST("a = 1\na = 1 - 1\nreturn a", {
    assert(HNumber::Cast(result)->value() == 0);
  })

  FUN_TEST("a() { a = 1\nreturn b() { scope a\na = a + 1\nreturn a} }\n"
           "c = a()\nreturn c() + c() + c()", {
    assert(HNumber::Cast(result)->value() == 9);
  });

  // Unary ops
  FUN_TEST("a = 1\nreturn ++a", {
    assert(HNumber::Cast(result)->value() == 2);
  })

  FUN_TEST("a = 1\nreturn a++ + a", {
    assert(HNumber::Cast(result)->value() == 3);
  })

  // Runtime errors
  FUN_TEST("() {}", {
    assert(s.CaughtException() == true);
  })
TEST_END("functional test")
