#include "test.h"

TEST_START("API test")
  FUN_TEST("return (a) {\nreturn a + 2\n}", {
    assert(result->Is<Function>());

    Value* argv[1] = { Number::NewIntegral(1) };
    Value* num = result->As<Function>()->Call(NULL, 1, argv);
    assert(num->As<Number>()->Value() == 3);
  })
TEST_END("API test")
