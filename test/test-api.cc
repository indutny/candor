#include "test.h"

TEST_START("API test")
  Script s;

  s.Compile("a = b + c + d + e + f + 1", 25);
  s.Run();
TEST_END("API test")
