#include "test.h"

TEST_START("API test")
  Isolate i;

  {
    HandleScope scope;

    Handle<String> str(String::New("str", 3));
    {
      HandleScope scope;
      Handle<Number> num(Number::New(static_cast<double>(1)));

      double x = num->Value();
    }

    uint32_t len = str->Length();
  }
TEST_END("API test")
