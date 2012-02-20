#include "test.h"

TEST_START("API test")
  Script s;

  s.Compile("a = 32", 6);
  s.Run();
TEST_END("API test")
