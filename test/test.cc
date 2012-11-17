#include "test.h"
#include "test-list.h"

#define TEST_RUN(name) \
    fprintf(stdout, "-- ctest: %s --\n", #name); \
    __test_runner_##name();

#define TEST_SWITCH(name) \
    if (strcmp(argv[1], #name) == 0) { \
      TEST_RUN(name) \
    } else \

int main(int argc, char** argv) {
  if (argc == 1) {
    TESTS_ENUM(TEST_RUN)
    return 0;
  }

  TESTS_ENUM(TEST_SWITCH) {
    fprintf(stderr, "Test: %s was not found\n", argv[1]);
    exit(1);
  }

  return 0;
}
