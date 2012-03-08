#include "test.h"

static Value* Callback(uint32_t argc, Arguments& argv) {
  HandleScope scope;

  Handle<Number> lhs(argv[0]->As<Number>());
  Handle<Number> rhs(argv[1]->As<Number>());

  int64_t lhs_value = lhs->IntegralValue();
  int64_t rhs_value = rhs->IntegralValue();

  return Number::NewIntegral(lhs_value + 2 * rhs_value);
}

TEST_START("API test")
  FUN_TEST("return (a, b, c) {\n__$gc()\nreturn a + b + c(1, 2) + 2\n}", {
    assert(result->Is<Function>());

    Value* argv[3];
    argv[0] = Number::NewIntegral(1);
    argv[1] = Number::NewIntegral(2);
    argv[2] = Function::New(Callback);

    Value* num = result->As<Function>()->Call(NULL, 3, argv);
    assert(num->As<Number>()->Value() == 10);
  })
TEST_END("API test")
