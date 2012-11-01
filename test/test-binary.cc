#include "test.h"

TEST_START(binary)
  FUN_TEST("return 1 + 2", {
    assert(result->As<Number>()->Value() == 3);
  })

  FUN_TEST("return 1 + 2 * 3 + 4 / 2 + (3 | 2) + (5 & 3) + (3 ^ 2)", {
    assert(result->As<Number>()->Value() == 14);
  })

  FUN_TEST("return 1.0 + 2.0 * 3.0 + 4.0 / 2.0 + (3.0 | 2.0) + "
           "(5.0 & 3.0) + (3.0 ^ 2.0)", {
    assert(result->As<Number>()->Value() == 14);
  })

  FUN_TEST("a = 1\na = 1 - 1\nreturn a", {
    assert(result->As<Number>()->Value() == 0);
  })

  FUN_TEST("a() { x = 1\nreturn b() { x = x + 1\nreturn x} }\n"
           "c = a()\nreturn c() + c() + c()", {
    assert(result->As<Number>()->Value() == 9);
  });

  FUN_TEST("return nil + nil", {
    assert(result->As<Number>()->Value() == 0);
  })

  FUN_TEST("return nil == nil", {
    assert(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("return nil != nil", {
    assert(result->As<Boolean>()->IsFalse());
  })

  // heap + unboxed, unboxed + heap

  FUN_TEST("return 1 + 1.0", {
    assert(result->As<Number>()->Value() == 2);
  })

  FUN_TEST("return 1.0 + 1", {
    assert(result->As<Number>()->Value() == 2);
  })

  // Equality

  FUN_TEST("return nil === {}", {
    assert(result->As<Boolean>()->IsFalse());
  })

  FUN_TEST("return nil !== {}", {
    assert(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("return 0 !== {}", {
    assert(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("return 'abc' == 'abc'", {
    assert(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("return 'abc' == 'abd'", {
    assert(result->As<Boolean>()->IsFalse());
  })

  FUN_TEST("a() {}\nreturn a == a", {
    assert(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("a() {}\nreturn a == 1", {
    assert(result->As<Boolean>()->IsFalse());
  })

  // Not-strict (with coercing) equality
  FUN_TEST("return 0 == []", {
    assert(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("return 0 != []", {
    assert(result->As<Boolean>()->IsFalse());
  })

  FUN_TEST("return '123' == 123", {
    assert(result->As<Boolean>()->IsTrue());
  })

  // Comparison
  FUN_TEST("return '123' >= 100", {
    assert(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("return 123 <= '100'", {
    assert(result->As<Boolean>()->IsFalse());
  })

  FUN_TEST("return true > false'", {
    assert(result->As<Boolean>()->IsTrue());
  })

  // Binary logic
  FUN_TEST("if (true || false) { return true }", {
    assert(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("if (1 || false) { return true }", {
    assert(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("if (1 && 'xyz') { return true }", {
    assert(result->As<Boolean>()->IsTrue());
  })

  // Math

  FUN_TEST("return 3 - '1'", {
    assert(result->As<Number>()->Value() == 2);
  })

  FUN_TEST("return 'hello ' + 'world'", {
    String* str = result->As<String>();
    assert(str->Length() == 11);
    assert(strncmp(str->Value(), "hello world", str->Length()) == 0);
  })

  FUN_TEST("return '1' + 1", {
    String* str = result->As<String>();
    assert(str->Length() == 2);
    assert(strncmp(str->Value(), "11", str->Length()) == 0);
  })

  FUN_TEST("return '1' - {}", {
    assert(result->As<Number>()->Value() == 1);
  })

  FUN_TEST("return '5' & 3", {
    assert(result->As<Number>()->Value() == 1);
  })
TEST_END(binary)
