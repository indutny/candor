#ifndef _SRC_RUNTIME_H_
#define _SRC_RUNTIME_H_

#include <stdint.h> // uint32_t

namespace dotlang {

// Forward declarations
class Heap;

typedef char* (*RuntimeAllocateCallback)(Heap* heap, uint32_t bytes);
char* RuntimeAllocate(Heap* heap, uint32_t bytes);

typedef char* (*RuntimeLookupPropertyCallback)(void* obj, void* key);
char* RuntimeLookupProperty(void* obj, void* key);

} // namespace dotlang

#endif // _SRC_RUNTIME_H_
