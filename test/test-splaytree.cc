#include <test.h>
#include <utils.h>
#include <splay-tree.h>
#include <stdlib.h>

static const int kKeyCount = 100000;

TEST_START(splaytree)
  {
    SplayTree<NumberKey, NumberKey, NopPolicy, EmptyClass> tree;

    // Sequential insertion
    for (int i = 0; i < kKeyCount; i++) {
      tree.Insert(NumberKey::New(i), NumberKey::New(i));
    }

    // Sequential read
    for (int i = 0; i < kKeyCount; i++) {
      ASSERT(tree.Find(NumberKey::New(i))->value() == i);
    }

    // Pseudo-random access
    int start = 0;
    int end = kKeyCount - 1;
    for (; start < end; start++, end--) {
      ASSERT(tree.Find(NumberKey::New(start))->value() == start);
      ASSERT(tree.Find(NumberKey::New(end))->value() == end);
    }

    // Random access
    srandom(13589);
    for (int i = 0; i < kKeyCount; i++) {
      int key = random() % kKeyCount;
      ASSERT(tree.Find(NumberKey::New(key))->value() == key);
    }
  }

  {
    SplayTree<NumberKey, NumberKey, NopPolicy, EmptyClass> tree;

    // Insert even keys
    for (int i = 0; i < kKeyCount; i += 2) {
      tree.Insert(NumberKey::New(i), NumberKey::New(i));
    }

    // Read odd keys
    for (int i = 1; i < kKeyCount; i += 2) {
      ASSERT(tree.Find(NumberKey::New(i))->value() == (i - 1));
    }
  }
TEST_END(splaytree)
