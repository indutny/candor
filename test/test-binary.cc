#include "test.h"

TEST_START(binary)
  FUN_TEST("return 1.0 + 2.0", {
    ASSERT(result->As<Number>()->Value() == 3);
  })

  FUN_TEST("return 1 + 2", {
    ASSERT(result->As<Number>()->Value() == 3);
  })

  FUN_TEST("return 1 + 2 * 3 + 4 / 2 + (3 | 2) + (5 & 3) + (3 ^ 2)", {
    ASSERT(result->As<Number>()->Value() == 14);
  })

  FUN_TEST("return 1.0 + 2.0 * 3.0 + 4.0 / 2.0 + (3.0 | 2.0) + "
           "(5.0 & 3.0) + (3.0 ^ 2.0)", {
    ASSERT(result->As<Number>()->Value() == 14);
  })

  FUN_TEST("a = 1\na = 1 - 1\nreturn a", {
    ASSERT(result->As<Number>()->Value() == 0);
  })

  FUN_TEST("a() { x = 1\nreturn b() { x = x + 1\nreturn x} }\n"
           "c = a()\nreturn c() + c() + c()", {
    ASSERT(result->As<Number>()->Value() == 9);
  });

  FUN_TEST("return nil + nil", {
    ASSERT(result->As<Number>()->Value() == 0);
  })

  FUN_TEST("return nil == nil", {
    ASSERT(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("return nil != nil", {
    ASSERT(result->As<Boolean>()->IsFalse());
  })

  // heap + unboxed, unboxed + heap

  FUN_TEST("return 1 + 1.0", {
    ASSERT(result->As<Number>()->Value() == 2);
  })

  FUN_TEST("return 1.0 + 1", {
    ASSERT(result->As<Number>()->Value() == 2);
  })

  // Equality

  FUN_TEST("return nil === {}", {
    ASSERT(result->As<Boolean>()->IsFalse());
  })

  FUN_TEST("return nil !== {}", {
    ASSERT(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("return 0 !== {}", {
    ASSERT(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("return 'abc' == 'abc'", {
    ASSERT(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("return 'abc' == 'abd'", {
    ASSERT(result->As<Boolean>()->IsFalse());
  })

  FUN_TEST("a() {}\nreturn a == a", {
    ASSERT(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("a() {}\nreturn a == 1", {
    ASSERT(result->As<Boolean>()->IsFalse());
  })

  // Not-strict (with coercing) equality
  FUN_TEST("return 0 == []", {
    ASSERT(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("return 0 != []", {
    ASSERT(result->As<Boolean>()->IsFalse());
  })

  FUN_TEST("return '123' == 123", {
    ASSERT(result->As<Boolean>()->IsTrue());
  })

  // Comparison
  FUN_TEST("return '123' >= 100", {
    ASSERT(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("return nil < 1", {
    ASSERT(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("return nil < 3", {
    ASSERT(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("return 123 <= '100'", {
    ASSERT(result->As<Boolean>()->IsFalse());
  })

  FUN_TEST("return true > false'", {
    ASSERT(result->As<Boolean>()->IsTrue());
  })

  // Binary logic
  FUN_TEST("if (true || false) { return true }", {
    ASSERT(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("if (1 || false) { return true }", {
    ASSERT(result->As<Boolean>()->IsTrue());
  })

  FUN_TEST("if (1 && 'xyz') { return true }", {
    ASSERT(result->As<Boolean>()->IsTrue());
  })

  // Math

  FUN_TEST("return 3 - '1'", {
    ASSERT(result->As<Number>()->Value() == 2);
  })

  FUN_TEST("return 'hello ' + 'world'", {
    String* str = result->As<String>();
    ASSERT(str->Length() == 11);
    ASSERT(strncmp(str->Value(), "hello world", str->Length()) == 0);
  })

  FUN_TEST("return '1' + 1", {
    String* str = result->As<String>();
    ASSERT(str->Length() == 2);
    ASSERT(strncmp(str->Value(), "11", str->Length()) == 0);
  })

  FUN_TEST("return '1' - {}", {
    ASSERT(result->As<Number>()->Value() == 1);
  })

  FUN_TEST("return '5' & 3", {
    ASSERT(result->As<Number>()->Value() == 1);
  })

  // Regression
  FUN_TEST("return 1 % 0", {
    ASSERT(result->As<Number>()->Value() == 0);
  })

  FUN_TEST("return 1.0 % 0.0", {
    ASSERT(result->As<Number>()->Value() == 0);
  })
TEST_END(binary)
