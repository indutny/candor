#ifndef _SRC_RUNTIME_H_
#define _SRC_RUNTIME_H_

#include <stdint.h> // uint32_t
#include <sys/types.h> // size_t

namespace dotlang {

// Forward declarations
class Heap;

typedef char* (*RuntimeAllocateCallback)(Heap* heap, uint32_t bytes);
char* RuntimeAllocate(Heap* heap, uint32_t bytes);

typedef char* (*RuntimeLookupPropertyCallback)(char* obj, char* key);
char* RuntimeLookupProperty(char* obj, char* key);

typedef size_t (*RuntimeCompareCallback)(char* lhs, char* rhs);
size_t RuntimeCompare(char* lhs, char* rhs);

} // namespace dotlang

#endif // _SRC_RUNTIME_H_
