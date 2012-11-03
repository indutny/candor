#include "test.h"

static Value* Callback(uint32_t argc, Value* argv[]) {
  ASSERT(argc == 3);

  Handle<Number> lhs(argv[0]->As<Number>());
  Handle<Number> rhs(argv[1]->As<Number>());
  Handle<Function> fn(argv[2]->As<Function>());

  int64_t lhs_value = lhs->IntegralValue();
  int64_t rhs_value = rhs->IntegralValue();

  int64_t fn_ret = fn->Call(0, NULL)->As<Number>()->IntegralValue();

  return Number::NewIntegral(lhs_value + 2 * rhs_value + 3 * fn_ret);
}

static Value* ObjectCallback(uint32_t argc, Value* argv[]) {
  Object* obj = Object::New();
  obj->Set(String::New("y", 1), Number::NewIntegral(1234));

  return obj;
}

static Value* ArrayCallback(uint32_t argc, Value* argv[]) {
  Array* arr = Array::New();
  arr->Set(3, Number::NewIntegral(4));

  ASSERT(arr->Length() == 4);

  return arr;
}

static Value* FnThreeCallback(uint32_t argc, Value* argv[]) {
  ASSERT(argc == 3);

  return Nil::New();
}

static Value* FnTwoCallback(uint32_t argc, Value* argv[]) {
  ASSERT(argc == 2);

  return Nil::New();
}

static Value* PrintCallback(uint32_t argc, Value* argv[]) {
  ASSERT(argc == 1);

  return Nil::New();
}

static int weak_called = 0;

static void WeakCallback(Value* obj) {
  ASSERT(obj->Is<Object>());
  ASSERT(obj->As<Object>()->Get(String::New("key", 3))->
      As<Boolean>()->IsTrue());

  weak_called++;
}

static int weak_handle_called = 0;

static void WeakHandleCallback(Value* obj) {
  weak_handle_called++;
}

static Value* GetWeak(uint32_t argc, Value* argv[]) {
  ASSERT(argc == 0);

  Handle<Object> obj(Object::New());

  obj->Set(String::New("key", 3), Boolean::True());

  obj->SetWeakCallback(WeakCallback);

  Array* arr = Isolate::GetCurrent()->StackTrace();
  ASSERT(arr->Length() == 2);
  ASSERT(arr->Get(0)->As<Object>()->Get("line")->As<Number>()->Value() == 3);
  ASSERT(arr->Get(1)->As<Object>()->Get("line")->As<Number>()->Value() == 1);

  return *obj;
}

struct CDataStruct {
  int x;
  int y;
};

static Value* UseCDataCallback(uint32_t argc, Value* argv[]) {
  ASSERT(argc == 1);

  CDataStruct* s = reinterpret_cast<CDataStruct*>(
      argv[0]->As<CData>()->GetContents());

  ASSERT(s->x == 1);
  ASSERT(s->y == 2);

  return Nil::New();
}

static int wrapper_destroyed = 0;

class WrapTest : public CWrapper {
 public:
  WrapTest() : CWrapper(&magic) {
    x = 0;
    y = 1;
    z = 2;
    j = 3;
  }
  ~WrapTest() {
    wrapper_destroyed++;
  }

  int x, y, z, j;

  static const int magic;
};

const int WrapTest::magic = 0;

class SubWrapTest : public WrapTest {
 public:
  SubWrapTest() {
    k = 1;
  }

  int k;
};

static Value* GetWrapper(uint32_t argc, Value* argv[]) {
  ASSERT(argc == 0);

  SubWrapTest* w = new SubWrapTest();

  w->Ref();

  return w->Wrap();
}


static Value* Unref(uint32_t argc, Value* argv[]) {
  ASSERT(argc == 1);

  ASSERT(CWrapper::HasClass(argv[0], &WrapTest::magic));
  WrapTest* w = CWrapper::Unwrap<WrapTest>(argv[0]);

  w->Unref();

  ASSERT(w->x == 0);

  return w->Wrap();
}


static Value* Unwrap(uint32_t argc, Value* argv[]) {
  ASSERT(argc == 1);

  ASSERT(CWrapper::HasClass(argv[0], &WrapTest::magic));
  WrapTest* w = CWrapper::Unwrap<WrapTest>(argv[0]);

  ASSERT(w->j == 3);

  return w->Wrap();
}

TEST_START(api)
  FUN_TEST("return (a, b, c) {\n"
           "return a + b + c(1, 2, () { __$gc()\nreturn 3 }) + 2\n"
           "}", {
    Value* argv[3];
    argv[0] = Number::NewIntegral(1);
    argv[1] = Number::NewIntegral(2);
    argv[2] = Function::New(Callback);

    Value* num = result->As<Function>()->Call(3, argv);
    ASSERT(num->As<Number>()->Value() == 19);
  })

  FUN_TEST("return { a: 1, callback: function(obj) { return obj.a } }", {
    Handle<Object> obj(result->As<Object>());
    Handle<String> key(String::New("a", 1));

    Value* val = obj->Get(*key);

    ASSERT(val->As<Number>()->Value() == 1);

    obj->Set(*key, Number::NewIntegral(3));

    val = obj->Get(*key);

    ASSERT(val->As<Number>()->Value() == 3);

    Handle<String> callback_key(String::New("callback", 8));
    Handle<Function> callback(obj->Get(*callback_key)->As<Function>());

    Handle<Object> data(Object::New());
    data->Set(*key, Number::NewIntegral(1234));
    Value* argv[1] = { *data };

    Value* result = callback->Call(1, argv);
    ASSERT(result->As<Number>()->Value() == 1234);
  })

  FUN_TEST("return { a: 1, b: 2 }", {
    Array* keys = result->As<Object>()->Keys();

    ASSERT(keys->Length() == 2);
  })

  FUN_TEST("return { a: 1, b: 2 }", {
    Object* clone = result->As<Object>()->Clone();

    ASSERT(clone->Get("a")->As<Number>()->Value() == 1);
    ASSERT(clone->Get("b")->As<Number>()->Value() == 2);
  })

  FUN_TEST("return () { return global.g }", {
    Handle<Object> global(Object::New());
    global->Set(String::New("g", 1), Number::NewIntegral(1234));

    Function* fn = result->As<Function>();
    fn->SetContext(*global);

    Value* ret = fn->Call(0, NULL);
    ASSERT(ret->As<Number>()->Value() == 1234);
  })

  FUN_TEST("x = { p: 1234 }\nreturn () { __$gc()\nreturn x.p }", {

    Function* fn = result->As<Function>();

    Value* ret = fn->Call(0, NULL);
    ASSERT(ret->As<Number>()->Value() == 1234);
  })

  FUN_TEST("return 1", {
    String* str = result->ToString();

    ASSERT(str->Length() == 1);
    ASSERT(strncmp(str->Value(), "1", 1) == 0);
  })

  FUN_TEST("return (x) { return x().y }", {
    Value* argv[1] = { Function::New(ObjectCallback) };
    Value* ret = result->As<Function>()->Call(1, argv);
    ASSERT(ret->As<Number>()->Value() == 1234);
  })

  FUN_TEST("return (x) { return x()[3] }", {
    Value* argv[1] = { Function::New(ArrayCallback) };
    Value* ret = result->As<Function>()->Call(1, argv);
    ASSERT(ret->As<Number>()->Value() == 4);
  })

  FUN_TEST("return (fn1, fn2) { return fn1(fn2(1, 2), 1, 2) }", {
    Value* argv[2];
    argv[0] = Function::New(FnThreeCallback);
    argv[1] = Function::New(FnTwoCallback);
    Value* ret = result->As<Function>()->Call(2, argv);
    ASSERT(ret->Is<Nil>());
  })

  {
    Isolate i;
    const char* code = "get = global.get\n"
                       "(() {\n"
                       "  x = get()\n"
                       "})()\n"
                       "__$gc()\n__$gc()";

    Function* f = Function::New("api", code, strlen(code));

    Object* global = Object::New();
    global->Set(String::New("get", 3), Function::New(GetWeak));

    f->SetContext(global);

    Value* ret = f->Call(0, NULL);
    ASSERT(ret->Is<Nil>());
    ASSERT(weak_called == 1);
  }

  {
    Isolate i;
    const char* code = "return () {\n__$gc()\n__$gc()\n__$gc()\n}";

    Function* f = Function::New("api", code, strlen(code));

    Handle<Object> weak(Object::New());
    weak.Unref();
    weak->SetWeakCallback(WeakHandleCallback);

    Value* ret = f->Call(0, NULL);

    // Call gc
    ret->As<Function>()->Call(0, NULL);

    ASSERT(weak_handle_called == 1);
  }

  // CData
  {
    Isolate i;
    const char* code = "global.use(global.data)";

    Function* f = Function::New("api", code, strlen(code));

    CDataStruct* s;
    CData* data = CData::New(sizeof(*s));
    s = reinterpret_cast<CDataStruct*>(data->GetContents());

    s->x = 1;
    s->y = 2;

    Object* global = Object::New();
    global->Set(String::New("use", 3), Function::New(UseCDataCallback));
    global->Set(String::New("data", 4), data);

    f->SetContext(global);

    Value* ret = f->Call(0, NULL);
    ASSERT(ret->Is<Nil>());
  }

  // CWrapper
  {
    Isolate i;
    const char* code = "get = global.get\n"
                       "unref = global.unref\n"
                       "unwrap = global.unwrap\n"
                       "(() {\n"
                       "  x = get()\n"
                       "  __$gc()\n"
                       "  unwrap(x)\n"
                       "  unref(x)\n"
                       "})()\n"
                       "__$gc()\n__$gc()";

    Function* f = Function::New("api", code, strlen(code));

    Object* global = Object::New();
    global->Set(String::New("get", 3), Function::New(GetWrapper));
    global->Set(String::New("unref", 5), Function::New(Unref));
    global->Set(String::New("unwrap", 6), Function::New(Unwrap));

    f->SetContext(global);

    Value* ret = f->Call(0, NULL);
    ASSERT(ret->Is<Nil>());
    ASSERT(wrapper_destroyed == 1);
  }

  // Regressions
  {
    Isolate i;
    const char* code = "print = global.print\n"
                       "fn() {\n"
                       "  return print\n"
                       "}\n"
                       "__$gc()\n"
                       "return fn()";

    Function* f = Function::New("api", code, strlen(code));

    Object* global = Object::New();
    global->Set(String::New("print", 5), Function::New(PrintCallback));

    f->SetContext(global);

    Value* ret = f->Call(0, NULL);
    ASSERT(ret->Is<Function>());
  }
TEST_END(api)
