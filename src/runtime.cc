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
