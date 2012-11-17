#include "test.h"

TEST_START(functional)
  // Objects
  FUN_TEST("a = {a:1,b:2,c:3}\nreturn a.c", {
    ASSERT(result->As<Number>()->Value() == 3);
  })

  FUN_TEST("return ({a:1,b:2,c:3}).c", {
    ASSERT(result->As<Number>()->Value() == 3);
  })

  // While
  FUN_TEST("i = 10\n"
           "while (i--) {}\n"
           "return i", {
    ASSERT(result->As<Number>()->Value() == -1);
  })

  FUN_TEST("i = 10\nj = 0\n"
           "while (i--) { j = j + 1\n}\n"
           "return j", {
    ASSERT(result->As<Number>()->Value() == 10);
  })

  FUN_TEST("i = 10\nk = 0\n"
           "while (--i) {\n"
           "  j = 10\n"
           "  while (--j) {\n"
           "    k = k + 1\n"
           "  }\n"
           "}\n"
           "return k", {
    ASSERT(result->As<Number>()->Value() == 81);
  })

  FUN_TEST("i = 10\nj = 0\nk = 0\n"
           "while (--i) {\n"
           "  j = 10\n"
           "  while (--j) {\n"
           "    k = k + 1\n"
           "  }\n"
           "}\n"
           "return k", {
    ASSERT(result->As<Number>()->Value() == 81);
  })

  FUN_TEST("i = 10\nj = 0\nk = 0\nl = 0\n"
           "while (--i) {\n"
           "  j = 10\n"
           "  k = 0\n"
           "  while (--j) {\n"
           "    k = 10\n"
           "    while (--k) {\n"
          "       l = l + 1\n"
          "     }\n"
           "  }\n"
           "}\n"
           "return l", {
    ASSERT(result->As<Number>()->Value() == 729);
  })

  FUN_TEST("i = 3\na = 0\n"
           "while (--i) {\n"
           "  j = 3\n"
           "  while (--j) {\n"
           "    a = { x : { y : a } }\n"
           "  }\n"
           "}\n"
           "return a.x.y", {
    ASSERT(result->Is<Object>());
  })

  // Functions
  FUN_TEST("a() {}\nreturn a", {
    ASSERT(result->Is<Function>());
  })

  FUN_TEST("a() { return 1 }\nreturn a()", {
    ASSERT(result->As<Number>()->Value() == 1);
  })

  FUN_TEST("a(x) { return x }\nreturn a(1)", {
    ASSERT(result->As<Number>()->Value() == 1);
  })

  // Regression
  FUN_TEST("a() { return 1 }\nreturn a({})", {
    ASSERT(result->As<Number>()->Value() == 1);
  })

  FUN_TEST("a() { return 1 }\nreturn a('' + 1)", {
    ASSERT(result->As<Number>()->Value() == 1);
  })

  FUN_TEST("a(x) { return x }\nreturn a('' + 1)", {
    ASSERT(result->ToNumber()->Value() == 1);
  })

  FUN_TEST("a() { return 1 }\nreturn a('' + 1, '' + 1, '' + 1, ''+ 1)", {
    ASSERT(result->As<Number>()->Value() == 1);
  })

  FUN_TEST("a(b) { return b }\nreturn a(3) + a(4)", {
    ASSERT(result->As<Number>()->Value() == 7);
  })

  FUN_TEST("a(b) { return b }\nreturn a()", {
    ASSERT(result->Is<Nil>());
  })

  FUN_TEST("a(b, c) { return b + 2 * c }\nreturn a(1, 2)", {
    ASSERT(result->As<Number>()->Value() == 5);
  })

  FUN_TEST("a(b, c) { return b + 2 * c }\nreturn a(a(3, 4), 2)", {
    ASSERT(result->As<Number>()->Value() == 15);
  })

  FUN_TEST("b() {\nreturn 1\n}\na(c) {\nreturn c()\n}\nreturn a(b)", {
    ASSERT(result->As<Number>()->Value() == 1);
  })

  FUN_TEST("a(c) {\nreturn c()\n}\nreturn a(() {\nreturn 1\n})", {
    ASSERT(result->As<Number>()->Value() == 1);
  })

  // Context slots
  FUN_TEST("b = 13589\na() { b }\nreturn b", {
    ASSERT(result->As<Number>()->Value() == 13589);
  })

  FUN_TEST("a() { b = 1234 }\nb = 13589\na()\nreturn b", {
    ASSERT(result->As<Number>()->Value() == 1234);
  })

  FUN_TEST("a(x) { return b() { return x} }\nreturn a(1)()", {
    ASSERT(result->As<Number>()->Value() == 1);
  });

  FUN_TEST("return ((x) { "
           "  y = x\n"
           "  return b() { x\nreturn y(2) }"
           "})((x) { return 2 * x })()", {
    ASSERT(result->As<Number>()->Value() == 4);
  });

  // Prefix
  FUN_TEST("return typeof nil", {
    String* str = result->As<String>();
    ASSERT(str->Length() == 3);
    ASSERT(strncmp(str->Value(), "nil", str->Length()) == 0);
  })

  FUN_TEST("return typeof 1", {
    String* str = result->As<String>();
    ASSERT(str->Length() == 6);
    ASSERT(strncmp(str->Value(), "number", str->Length()) == 0);
  })

  FUN_TEST("return typeof '123'", {
    String* str = result->As<String>();
    ASSERT(str->Length() == 6);
    ASSERT(strncmp(str->Value(), "string", str->Length()) == 0);
  })

  FUN_TEST("return sizeof '123'", {
    ASSERT(result->As<Number>()->Value() == 3);
  })

  FUN_TEST("return keysof { a: 1, b: 2 }", {
    ASSERT(result->As<Array>()->Length() == 2);
    ASSERT(result->As<Array>()->Get(1)->Is<String>());
  })

  // Unary ops
  FUN_TEST("a = 1\nreturn ++a", {
    ASSERT(result->As<Number>()->Value() == 2);
  })

  FUN_TEST("a = 1\nreturn a++ + a", {
    ASSERT(result->As<Number>()->Value() == 3);
  })

  FUN_TEST("return !true", {
    ASSERT(result->As<Boolean>()->IsFalse());
  })

  FUN_TEST("return !false", {
    ASSERT(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("return !0", {
    ASSERT(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("return !nil", {
    ASSERT(result->As<Boolean>()->IsTrue());
  })

  // Objects
  FUN_TEST("return {}", {
    ASSERT(result->Is<Object>());
  })

  FUN_TEST("return { a : 1 }", {
    ASSERT(result->Is<Object>());
  })

  FUN_TEST("a = {a:1,b:2,c:3,d:4,e:5,f:6,g:7}\n"
           "return a.a + a.b + a.c + a.d + a.e + a.f + a.g", {
    ASSERT(result->As<Number>()->Value() == 28);
  })

  FUN_TEST("a = {}\na.x = 1\nreturn a.x", {
    ASSERT(result->As<Number>()->Value() == 1);
  })

  // Nil slot lookup
  FUN_TEST("a.x = 1", {
    ASSERT(result->Is<Nil>());
  })

  FUN_TEST("a = { a : { b : 1 } }\n"
           "a = { x: { y: a } }\n"
           "return a.x.y.a.b", {
    ASSERT(result->As<Number>()->Value() == 1);
  })

  // Rehash and growing
  FUN_TEST("a = {}\n"
           "a.a = a.b = a.c = a.d = a.e = a.f = a.g = a.h = 1\n"
           "return a.a + a.b + a.c + a.d + a.e + a.f + a.g + a.h", {
    ASSERT(result->As<Number>()->Value() == 8);
  })

  FUN_TEST("a = { a: 1, b: 2 }\nreturn a.c", {
    ASSERT(result->Is<Nil>());
  })

  FUN_TEST("a = { a: 1, b: 2 }\nreturn a.c = (2 + a.a) + a.b", {
    ASSERT(result->As<Number>()->Value() == 5);
  })

  FUN_TEST("a = { a: { b: 2 } }\nreturn a.a.b", {
    ASSERT(result->As<Number>()->Value() == 2);
  })

  FUN_TEST("key() {\nreturn 'key'\n}\na = { key: 2 }\nreturn a[key()]", {
    ASSERT(result->As<Number>()->Value() == 2);
  })

  // Numeric keys
  FUN_TEST("a = { 1: 2, 2: 3, '1': 2, '2': 3}\n"
           "return a[1] + a[2] + a['1'] + a['2'] + a[1.0] + a[2.0]", {
    ASSERT(result->As<Number>()->Value() == 15);
  });

  FUN_TEST("a = { 1.1: 2, 2.2: 3}\n"
           "return a[1.1] + a[2.2]", {
    ASSERT(result->As<Number>()->Value() == 5);
  });

  // Arrays
  FUN_TEST("a = [ 1, 2, 3, 4 ]\nreturn a[0] + a[1] + a[2] + a[3]", {
    ASSERT(result->As<Number>()->Value() == 10);
  })

  FUN_TEST("a = [ 1, 2, 3, 4 ]\nreturn sizeof a", {
    ASSERT(result->As<Number>()->Value() == 4);
  })

  FUN_TEST("a = [ 1, 2, 3, 4 ]\nreturn typeof a", {
    String* str = result->As<String>();
    ASSERT(str->Length() == 5);
    ASSERT(strncmp(str->Value(), "array", str->Length()) == 0);
  })

  // Global lookup
  FUN_TEST("global.a = 1\nreturn global.a", {
    ASSERT(result->As<Number>()->Value() == 1);
  })
TEST_END(functional)
