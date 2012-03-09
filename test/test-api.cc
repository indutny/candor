#include "test.h"

static Value* Callback(uint32_t argc, Arguments& argv) {
  assert(argc == 3);

  Handle<Number> lhs(argv[0]->As<Number>());
  Handle<Number> rhs(argv[1]->As<Number>());
  Handle<Function> fn(argv[2]->As<Function>());

  int64_t lhs_value = lhs->IntegralValue();
  int64_t rhs_value = rhs->IntegralValue();

  Value* fargv[0];
  int64_t fn_ret = fn->Call(NULL, 0, fargv)->As<Number>()->IntegralValue();

  return Number::NewIntegral(lhs_value + 2 * rhs_value + 3 * fn_ret);
}

static Value* ObjectCallback(uint32_t argc, Arguments& argv) {
  Object* obj = Object::New();
  obj->Set(String::New("y", 1), Number::NewIntegral(1234));

  return obj;
}

static Value* FnThreeCallback(uint32_t argc, Arguments& argv) {
  assert(argc == 3);

  return Nil::New();
}

static Value* FnTwoCallback(uint32_t argc, Arguments& argv) {
  assert(argc == 2);

  return Nil::New();
}

static Value* FnCallback(uint32_t argc, Arguments& argv) {
  assert(argc == 1);
  assert(argv[0]->Is<Function>());

  return Nil::New();
}

static Value* PrintCallback(uint32_t argc, Arguments& argv) {
  assert(argc == 1);

  return Nil::New();
}

static int weak_called = 0;

static void WeakCallback(Object* obj) {
  assert(obj->Is<Object>());
  assert(obj->Get(String::New("key", 3))->As<Boolean>()->IsTrue());

  weak_called++;
}

static Value* GetWeak(uint32_t argc, Arguments& argv) {
  assert(argc == 0);

  Handle<Object> obj(Object::New());

  obj->Set(String::New("key", 3), Boolean::True());

  obj.SetWeakCallback(WeakCallback);

  return *obj;
}

TEST_START("API test")
  FUN_TEST("return (a, b, c) {\n"
           "return a + b + c(1, 2, () { __$gc()\nreturn 3 }) + 2\n"
           "}", {
    Value* argv[3];
    argv[0] = Number::NewIntegral(1);
    argv[1] = Number::NewIntegral(2);
    argv[2] = Function::New(Callback);

    Value* num = result->As<Function>()->Call(NULL, 3, argv);
    assert(num->As<Number>()->Value() == 19);
  })

  FUN_TEST("return { a: 1, callback: function(obj) { return obj.a } }", {
    Handle<Object> obj(result->As<Object>());
    Handle<String> key(String::New("a", 1));

    Value* val = obj->Get(*key);

    assert(val->As<Number>()->Value() == 1);

    obj->Set(*key, Number::NewIntegral(3));

    val = obj->Get(*key);

    assert(val->As<Number>()->Value() == 3);

    Handle<String> callback_key(String::New("callback", 8));
    Handle<Function> callback(obj->Get(*callback_key)->As<Function>());

    Handle<Object> data(Object::New());
    data->Set(*key, Number::NewIntegral(1234));
    Value* argv[1] = { *data };

    Value* result = callback->Call(NULL, 1, argv);
    assert(result->As<Number>()->Value() == 1234);
  })

  FUN_TEST("return () { scope g\n return g }", {
    Value* argv[0];

    Handle<Object> global(Object::New());
    global->Set(String::New("g", 1), Number::NewIntegral(1234));

    Value* ret = result->As<Function>()->Call(*global, 0, argv);
    assert(ret->As<Number>()->Value() == 1234);
  })

  FUN_TEST("return 1", {
    String* str = result->ToString();

    assert(str->Length() == 1);
    assert(strncmp(str->Value(), "1", 1) == 0);
  })

  FUN_TEST("return (x) { return x().y }", {
    Value* argv[1] = { Function::New(ObjectCallback) };
    Value* ret = result->As<Function>()->Call(NULL, 1, argv);
    assert(ret->As<Number>()->Value() == 1234);
  })

  FUN_TEST("return (fn1, fn2) { return fn1(fn2(1, 2), 1, 2) }", {
    Value* argv[2];
    argv[0] = Function::New(FnThreeCallback);
    argv[1] = Function::New(FnTwoCallback);
    Value* ret = result->As<Function>()->Call(NULL, 2, argv);
    assert(ret->Is<Nil>());
  })

  {
    Isolate i;
    const char* code = "(() {\n"
                       "  scope get\n"
                       "  x = get()\n"
                       "})()\n"
                       "__$gc()\n__$gc()";

    Function* f = Function::New(code, strlen(code));

    Object* global = Object::New();
    global->Set(String::New("get", 3), Function::New(GetWeak));

    Value* argv[0];
    Value* ret = f->Call(global, 0, argv);
    assert(ret->Is<Nil>());
    assert(weak_called == 1);
  }

  // Regressions
  {
    Isolate i;
    const char* code = "((fn) {\n"
                       "  scope print\n"
                       "  print(fn)\n"
                       "  fn2 = fn\n"
                       "  return () {\n"
                       "    scope fn, fn2, print\n"
                       "    print(fn2)\n"
                       "  }\n"
                       "})(() {\n"
                       "})()";

    Function* f = Function::New(code, strlen(code));

    Object* global = Object::New();
    global->Set(String::New("print", 5), Function::New(FnCallback));

    Value* argv[0];
    Value* ret = f->Call(global, 0, argv);
    assert(ret->Is<Nil>());
  }

  {
    Isolate i;
    const char* code = "fn() {\n"
                       "  scope print\n"
                       "  return print\n"
                       "}\n"
                       "__$gc()\n"
                       "return fn()";

    Function* f = Function::New(code, strlen(code));

    Object* global = Object::New();
    global->Set(String::New("print", 5), Function::New(PrintCallback));

    Value* argv[0];
    Value* ret = f->Call(global, 0, argv);
    assert(ret->Is<Function>());
  }
TEST_END("API test")
