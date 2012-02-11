#include "test.h"

TEST_START("API test")
  Script s;

  s.Compile("a = 1", 5);
  s.Run();
TEST_END("API test")
