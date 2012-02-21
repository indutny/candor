#include "test.h"

TEST_START("Functional test")
  Script s;

  s.Compile("a = 32", 6);
  s.Run();
TEST_END("Functional test")
