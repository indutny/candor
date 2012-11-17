#include <test.h>
#include <utils.h>
#include <list.h>
#include <stdlib.h>

static const int kItemCount = 1003;

TEST_START(list)
  // Push and pop
  {
    SortableList<NumberKey, NopPolicy, EmptyClass> list(10);

    // Sequential insertion
    for (int i = 0; i < kItemCount; i++) {
      list.Push(NumberKey::New(i));
    }

    // Sequential read
    for (int i = kItemCount - 1; i >= 0; i--) {
      ASSERT(list.Pop()->value() == i);
    }

    ASSERT(list.length() == 0);
  }

  // Push and shift
  {
    SortableList<NumberKey, NopPolicy, EmptyClass> list(10);

    // Sequential insertion
    for (int i = 0; i < kItemCount; i++) {
      list.Push(NumberKey::New(i));
    }

    // Sequential read
    for (int i = 0; i < kItemCount; i++) {
      ASSERT(list.Shift()->value() == i);
    }

    ASSERT(list.length() == 0);
  }

  // Unshift and pop
  {
    SortableList<NumberKey, NopPolicy, EmptyClass> list(10);

    // Sequential insertion
    for (int i = 0; i < kItemCount; i++) {
      list.Unshift(NumberKey::New(i));
    }

    // Sequential read
    for (int i = 0; i < kItemCount; i++) {
      ASSERT(list.Pop()->value() == i);
    }

    ASSERT(list.length() == 0);
  }

  // Sort and remove
  {
    SortableList<NumberKey, NopPolicy, EmptyClass> list(10);

    // Insert in reverse order
    for (int i = kItemCount - 1; i >= 0; i--) {
      list.Push(NumberKey::New(i));
    }

    // Sort list
    list.Sort<NumberKey>();

    // Remove one item
    list.RemoveAt(kItemCount >> 1);

    // Insert item back in list
    list.InsertSorted<NumberKey>(NumberKey::New(kItemCount >> 1));

    // Sequential read
    for (int i = 0; i < kItemCount; i++) {
      ASSERT(list.At(i)->value() == i);
    }

    // Remove one item again
    list.RemoveAt(10);

    // Push it to the end
    list.Push(NumberKey::New(10));

    // Sort already sorted list
    list.Sort<NumberKey>();

    // Sequential read
    for (int i = 0; i < kItemCount; i++) {
      ASSERT(list.Shift()->value() == i);
    }

    ASSERT(list.length() == 0);
  }
TEST_END(list)
