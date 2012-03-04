#include "test.h"

TEST_START("binary operations test")
  FUN_TEST("return 1 + 2 * 3 + 4 / 2 + (3 | 2) + (5 & 3) + (3 ^ 2)", {
    assert(HValue::As<HNumber>(result)->value() == 14);
  })

  FUN_TEST("return 1.0 + 2.0 * 3.0 + 4.0 / 2.0 + (3.0 | 2.0) + "
           "(5.0 & 3.0) + (3.0 ^ 2.0)", {
    assert(HValue::As<HNumber>(result)->value() == 14);
  })

  FUN_TEST("a = 1\na = 1 - 1\nreturn a", {
    assert(HValue::As<HNumber>(result)->value() == 0);
  })

  FUN_TEST("a() { a = 1\nreturn b() { scope a\na = a + 1\nreturn a} }\n"
           "c = a()\nreturn c() + c() + c()", {
    assert(HValue::As<HNumber>(result)->value() == 9);
  });

  FUN_TEST("return nil + nil", {
    assert(HValue::As<HNumber>(result)->value() == 0);
  })

  FUN_TEST("return nil == nil", {
    assert(HValue::As<HBoolean>(result)->is_true());
  })

  FUN_TEST("return nil != nil", {
    assert(HValue::As<HBoolean>(result)->is_false());
  })

  // Equality

  FUN_TEST("return nil === {}", {
    assert(HValue::As<HBoolean>(result)->is_false());
  })

  FUN_TEST("return nil !== {}", {
    assert(HValue::As<HBoolean>(result)->is_true());
  })

  FUN_TEST("return 0 !== {}", {
    assert(HValue::As<HBoolean>(result)->is_true());
  })

  FUN_TEST("return 'abc' == 'abc'", {
      assert(HValue::As<HBoolean>(result)->is_true());
  })

  FUN_TEST("return 'abc' == 'abd'", {
      assert(HValue::As<HBoolean>(result)->is_false());
  })

  // Not-strict (with coercing) equality
  FUN_TEST("return 0 == []", {
    assert(HValue::As<HBoolean>(result)->is_true());
  })

  FUN_TEST("return 0 != []", {
    assert(HValue::As<HBoolean>(result)->is_false());
  })

  FUN_TEST("return '123' == 123", {
    assert(HValue::As<HBoolean>(result)->is_true());
  })
TEST_END("binary operations test")
