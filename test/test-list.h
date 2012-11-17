#ifndef _TEST_TEST_LIST_H_
#define _TEST_TEST_LIST_H_

#define TESTS_ENUM(V)\
    V(api)\
    V(binary)\
    V(functional)\
    V(gc)\
    V(numbers)\
    V(parser)\
    V(scope)\
    V(fullgen) \
    V(hir) \
    V(lir) \
    V(splaytree) \
    V(list)

#define TEST_DECLARE(name)\
    int __test_runner_##name();
TESTS_ENUM(TEST_DECLARE)
#undef TEST_DECLARE

#endif // _TEST_TEST_LIST_H_
