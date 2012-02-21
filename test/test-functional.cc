#include "test.h"

TEST_START("functional test")
  Script s;

  const char* script = "a = 32\r\nreturn a";
  s.Compile(script, strlen(script));
  void* result = s.Run();
  assert(result != NULL);
  assert(HNumber::Cast(result)->value() == 32);

TEST_END("functional test")
