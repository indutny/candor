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
    RuntimeGrowObject(heap, obj);
    return RuntimeLookupProperty(heap, obj, key, insert);
  }

  if (insert) {
    *reinterpret_cast<char**>(space + index) = key;
  }

  return space + index + (mask + 8);
}


char* RuntimeGrowObject(Heap* heap, char* obj) {
  char** map_addr = reinterpret_cast<char**>(obj + 16);
  char* map = *map_addr;
  uint32_t size = *reinterpret_cast<uint32_t*>(map + 8);

  char* new_map = heap->AllocateTagged(Heap::kTagMap, 8 + (size << 5));
  // Set map size
  *(uint32_t*)(new_map + 8) = size << 1;

  // Fill new map with zeroes
  memset(new_map + 16, 0, size << 5);

  // Replace old map with a new
  *map_addr = new_map;

  // Change mask
  uint32_t mask = (size << 4) - 8;
  *reinterpret_cast<uint32_t*>(obj + 8) = mask;

  char* space = map + 16;

  // And rehash properties to new map
  for (uint32_t index = 0; index < size << 3; index += 8) {
    char* key= *reinterpret_cast<char**>(space + index);
    if (key== NULL) continue;

    char* value = *reinterpret_cast<char**>(space + index + (size << 3));
    assert(value != NULL);

    char* slot = RuntimeLookupProperty(heap, obj, key, true);
    *reinterpret_cast<char**>(slot) = value;
  }

  return 0;
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
