#include "test.h"

TEST_START(functional)
  // Basics: return + assign
  FUN_TEST("return 1", {
    assert(result->As<Number>()->Value() == 1);
  })

  FUN_TEST("return", {
    assert(result->Is<Nil>());
  })

  FUN_TEST("a = 32\nreturn a", {
    assert(result->As<Number>()->Value() == 32);
  })

  FUN_TEST("a = b = 32\nreturn a", {
    assert(result->As<Number>()->Value() == 32);
  })

  FUN_TEST("a = 32\nb = a\nreturn b", {
    assert(result->As<Number>()->Value() == 32);
  })

  FUN_TEST("a = nil\nreturn a", {
    assert(result->Is<Nil>());
  })

  // Spill test
  // NOTE: `{}` causes a stub call, which has side-effects
  // That means that every active register will be spilled and restored
  // after that call.
  FUN_TEST("a=1\nb=2\nc=3\nd=4\ne=5\nf=6\ng=7\nh=8\ni=9\nj=10\nk=11\nl=12\n"
           "x={}\n"
           "b\nc\nd\ne\nf\ng\nh\ni\nj\nk\nl\nreturn a", {
    assert(result->As<Number>()->Value() == 1);
  })

  FUN_TEST("return 'abcdef'", {
    String* str = result->As<String>();
    assert(str->Length() == 6);
    assert(strncmp(str->Value(), "abcdef", str->Length()) == 0);
  })

  // Boolean
  FUN_TEST("return true", {
    assert(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("return false", {
    assert(result->As<Boolean>()->IsFalse());
  })

  // Functions
  FUN_TEST("a() {}\nreturn a", {
    assert(result->Is<Function>());
  })

  FUN_TEST("a() { return 1 }\nreturn a()", {
    assert(result->As<Number>()->Value() == 1);
  })

  // Regression
  FUN_TEST("a() { return 1 }\nreturn a({})", {
    assert(result->As<Number>()->Value() == 1);
  })

  FUN_TEST("a() { return 1 }\nreturn a('' + 1)", {
    assert(result->As<Number>()->Value() == 1);
  })

  FUN_TEST("a(x) { return x }\nreturn a('' + 1)", {
    assert(result->ToNumber()->Value() == 1);
  })

  FUN_TEST("a() { return 1 }\nreturn a('' + 1, '' + 1, '' + 1, ''+ 1)", {
    assert(result->As<Number>()->Value() == 1);
  })

  FUN_TEST("a(b) { return b }\nreturn a(3) + a(4)", {
    assert(result->As<Number>()->Value() == 7);
  })

  FUN_TEST("a(b) { return b }\nreturn a()", {
    assert(result->Is<Nil>());
  })

  FUN_TEST("a(b, c) { return b + 2 * c }\nreturn a(1, 2)", {
    assert(result->As<Number>()->Value() == 5);
  })

  FUN_TEST("a(b, c) { return b + 2 * c }\nreturn a(a(3, 4), 2)", {
    assert(result->As<Number>()->Value() == 15);
  })

  FUN_TEST("b() {\nreturn 1\n}\na(c) {\nreturn c()\n}\nreturn a(b)", {
    assert(result->As<Number>()->Value() == 1);
  })

  FUN_TEST("a(c) {\nreturn c()\n}\nreturn a(() {\nreturn 1\n})", {
    assert(result->As<Number>()->Value() == 1);
  })

  // Context slots
  FUN_TEST("b = 13589\na() { b }\nreturn b", {
    assert(result->As<Number>()->Value() == 13589);
  })

  FUN_TEST("a() { a\nb = 1234 }\nb = 13589\na()\nreturn b", {
    assert(result->As<Number>()->Value() == 1234);
  })

  FUN_TEST("a(x) { return b() { return x} }\nreturn a(1)()", {
    assert(result->As<Number>()->Value() == 1);
  });

  FUN_TEST("return ((x) { "
           "  y = x\n"
           "  return b() { x\nreturn y(2) }"
           "})((x) { return 2 * x })()", {
    assert(result->As<Number>()->Value() == 4);
  });

  // Prefix
  FUN_TEST("return typeof nil", {
    String* str = result->As<String>();
    assert(str->Length() == 3);
    assert(strncmp(str->Value(), "nil", str->Length()) == 0);
  })

  FUN_TEST("return typeof 1", {
    String* str = result->As<String>();
    assert(str->Length() == 6);
    assert(strncmp(str->Value(), "number", str->Length()) == 0);
  })

  FUN_TEST("return typeof '123'", {
    String* str = result->As<String>();
    assert(str->Length() == 6);
    assert(strncmp(str->Value(), "string", str->Length()) == 0);
  })

  FUN_TEST("return sizeof '123'", {
    assert(result->As<Number>()->Value() == 3);
  })

  FUN_TEST("return keysof { a: 1, b: 2 }", {
    assert(result->As<Array>()->Length() == 2);
    assert(result->As<Array>()->Get(1)->Is<String>());
  })

  // Unary ops
  FUN_TEST("a = 1\nreturn ++a", {
    assert(result->As<Number>()->Value() == 2);
  })

  FUN_TEST("a = 1\nreturn a++ + a", {
    assert(result->As<Number>()->Value() == 3);
  })

  FUN_TEST("return !true", {
    assert(result->As<Boolean>()->IsFalse());
  })

  FUN_TEST("return !false", {
    assert(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("return !0", {
    assert(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("return !nil", {
    assert(result->As<Boolean>()->IsTrue());
  })

  // Objects
  FUN_TEST("return {}", {
    assert(result->Is<Object>());
  })

  FUN_TEST("return { a : 1 }", {
    assert(result->Is<Object>());
  })

  FUN_TEST("a = {a:1,b:2,c:3,d:4,e:5,f:6,g:7}\n"
           "return a.a + a.b + a.c + a.d + a.e + a.f + a.g", {
    assert(result->As<Number>()->Value() == 28);
  })

  // Nil slot lookup
  FUN_TEST("a.x = 1", {
    assert(result->Is<Nil>());
  })

  FUN_TEST("a = { a : { b : 1 } }\n"
           "a = { x: { y: a } }\n"
           "return a.x.y.a.b", {
    assert(result->As<Number>()->Value() == 1);
  })

  // Rehash and growing
  FUN_TEST("a = {}\n"
           "a.a = a.b = a.c = a.d = a.e = a.f = a.g = a.h = 1\n"
           "return a.a + a.b + a.c + a.d + a.e + a.f + a.g + a.h", {
    assert(result->As<Number>()->Value() == 8);
  })

  FUN_TEST("a = { a: 1, b: 2 }\nreturn a.c", {
    assert(result->Is<Nil>());
  })

  FUN_TEST("a = { a: 1, b: 2 }\nreturn a.c = (2 + a.a) + a.b", {
    assert(result->As<Number>()->Value() == 5);
  })

  FUN_TEST("a = { a: { b: 2 } }\nreturn a.a.b", {
    assert(result->As<Number>()->Value() == 2);
  })

  FUN_TEST("key() {\nreturn 'key'\n}\na = { key: 2 }\nreturn a[key()]", {
    assert(result->As<Number>()->Value() == 2);
  })

  // Numeric keys
  FUN_TEST("a = { 1: 2, 2: 3, '1': 2, '2': 3}\n"
           "return a[1] + a[2] + a['1'] + a['2'] + a[1.0] + a[2.0]", {
    assert(result->As<Number>()->Value() == 15);
  });

  FUN_TEST("a = { 1.1: 2, 2.2: 3}\n"
           "return a[1.1] + a[2.2]", {
    assert(result->As<Number>()->Value() == 5);
  });

  // Arrays
  FUN_TEST("a = [ 1, 2, 3, 4 ]\nreturn a[0] + a[1] + a[2] + a[3]", {
    assert(result->As<Number>()->Value() == 10);
  })

  FUN_TEST("a = [ 1, 2, 3, 4 ]\nreturn sizeof a", {
    assert(result->As<Number>()->Value() == 4);
  })

  FUN_TEST("a = [ 1, 2, 3, 4 ]\nreturn typeof a", {
    String* str = result->As<String>();
    assert(str->Length() == 5);
    assert(strncmp(str->Value(), "array", str->Length()) == 0);
  })

  // Global lookup
  FUN_TEST("global.a = 1\nreturn global.a", {
    assert(result->As<Number>()->Value() == 1);
  })

  // If
  FUN_TEST("if (true) {\n return 1\n} else {\nreturn 2\n}", {
    assert(result->As<Number>()->Value() == 1);
  })

  FUN_TEST("if (false) {\n return 1\n} else {\nreturn 2\n}", {
    assert(result->As<Number>()->Value() == 2);
  })

  FUN_TEST("if (1) {\n return 1\n} else {\nreturn 2\n}", {
    assert(result->As<Number>()->Value() == 1);
  })

  FUN_TEST("if (0) {\n return 1\n} else {\nreturn 2\n}", {
    assert(result->As<Number>()->Value() == 2);
  })

  FUN_TEST("if ('123') {\n return 1\n} else {\nreturn 2\n}", {
    assert(result->As<Number>()->Value() == 1);
  })

  FUN_TEST("if ('') {\n return 1\n} else {\nreturn 2\n}", {
    assert(result->As<Number>()->Value() == 2);
  })

  FUN_TEST("if (nil) {\n return 1\n} else {\nreturn 2\n}", {
    assert(result->As<Number>()->Value() == 2);
  })

  // While
  FUN_TEST("i = 10\nj = 0\n"
           "while (i--) { j = j + 1\n}\n"
           "return j", {
    assert(result->As<Number>()->Value() == 10);
  })
TEST_END(functional)
