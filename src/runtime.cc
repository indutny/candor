#include "runtime.h"
#include "heap.h" // Heap

#include <stdint.h> // uint32_t
#include <assert.h> // assert
#include <string.h> // strncmp
#include <sys/types.h> // size_t

namespace dotlang {

char* RuntimeAllocate(Heap* heap, uint32_t bytes) {
  return heap->new_space()->Allocate(bytes);
}


char* RuntimeLookupProperty(Heap* heap, char* obj, char* key, off_t insert) {
  char* map = *reinterpret_cast<char**>(obj + 16);
  char* space = map + 16;
  uint32_t mask = *reinterpret_cast<uint64_t*>(obj + 8);

  // Compute hash lazily
  uint32_t* hash_addr = reinterpret_cast<uint32_t*>(key + 8);
  uint32_t hash = *hash_addr;
  if (hash == 0) {
    hash = ComputeHash(key + 24, *reinterpret_cast<uint32_t*>(key + 16));
    *hash_addr = hash;
  }

  // Dive into space and walk it in circular manner
  uint32_t start = hash & mask;
  uint32_t end = start == 0 ? mask : start - 8;

  uint32_t index = start;
  char* key_slot = NULL;
  while (index != end) {
    key_slot = *reinterpret_cast<char**>(space + index);
    if (key_slot == NULL) break;
    if (RuntimeCompare(key_slot, key) == 0) break;

    index += 8;
    if (index > mask) index = 0;
  }

  // All key slots are filled - rehash and lookup again
  if (index == end) {
    assert(insert);
    return 0;
  }

  if (insert) {
    *reinterpret_cast<char**>(space + index) = key;
  }

  return space + index + (mask + 8);
}


size_t RuntimeCompare(char* lhs, char* rhs) {
  Heap::HeapTag lhs_tag = static_cast<Heap::HeapTag>(
      *reinterpret_cast<off_t*>(lhs));
  Heap::HeapTag rhs_tag = static_cast<Heap::HeapTag>(
      *reinterpret_cast<off_t*>(rhs));

  if (lhs_tag != Heap::kTagString) assert(0 && "Not implemented yet");
  if (rhs_tag != Heap::kTagString) assert(0 && "Not implemented yet");

  off_t lhs_length = *reinterpret_cast<off_t*>(lhs + 16);
  off_t rhs_length = *reinterpret_cast<off_t*>(rhs + 16);

  return lhs_length < rhs_length ? -1 :
         lhs_length > rhs_length ? 1 :
         strncmp(lhs + 24, rhs + 24, lhs_length);
}

} // namespace dotlang
