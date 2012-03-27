#include "test.h"
#include "test-list.h"

#define TEST_SWITCH(name)\
    if (strcmp(argv[1], #name) == 0) {\
      fprintf(stdout, "-- ctest: %s --\n", #name);\
      __test_runner_##name();\
    } else\

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: test/test test-name\n");
    exit(1);
  }

  TESTS_ENUM(TEST_SWITCH) {
    fprintf(stderr, "Test: %s was not found\n", argv[1]);
    exit(1);
  }
}
