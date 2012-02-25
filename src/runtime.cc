#include "runtime.h"
#include "heap.h" // Heap

#include <stdint.h> // uint32_t

namespace dotlang {

char* RuntimeAllocate(Heap* heap, uint32_t bytes) {
  return heap->new_space()->Allocate(bytes);
}

char* RuntimeLookupProperty(char* obj, char* key) {
  return reinterpret_cast<char*>(obj + 8);
}

} // namespace dotlang
