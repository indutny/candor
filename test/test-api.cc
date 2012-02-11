#include "test.h"

TEST_START("API test")
  Script s;

  s.Compile("a = b\nc = d\ne = f", 17);
  s.Run();
TEST_END("API test")
