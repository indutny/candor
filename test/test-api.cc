#include "test.h"

TEST_START("API test")
  FUN_TEST("return (a, b, c) {\n__$gc()\nreturn a + b + c + 2\n}", {
    assert(result->Is<Function>());

    Value* argv[3];
    argv[0] = Number::NewIntegral(1);
    argv[1] = Number::NewIntegral(2);
    argv[2] = Number::NewDouble(4);

    Value* num = result->As<Function>()->Call(NULL, 3, argv);
    assert(num->As<Number>()->Value() == 9);
  })
TEST_END("API test")
