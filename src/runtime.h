#ifndef _SRC_RUNTIME_H_
#define _SRC_RUNTIME_H_

#include "heap.h" // Heap, Heap::HeapTag
#include "heap-inl.h"
#include "ast.h" // BinOp

#include <stdint.h> // uint32_t
#include <unistd.h> // intptr_t
#include <sys/types.h> // size_t

namespace candor {
namespace internal {

// Wrapper for heap()->new_space()->Allocate()
typedef char* (*RuntimeAllocateCallback)(Heap* heap,
                                         uint32_t bytes);
char* RuntimeAllocate(Heap* heap, uint32_t bytes);

typedef void (*RuntimeCollectGarbageCallback)(Heap* heap, char* stack_top);
void RuntimeCollectGarbage(Heap* heap, char* stack_top);

typedef intptr_t (*RuntimeGetHashCallback)(Heap* heap, char* value);
intptr_t RuntimeGetHash(Heap* heap, char* value);

// Performs lookup into a hashmap
// if insert=1 - inserts key into map space
typedef intptr_t (*RuntimeLookupPropertyCallback)(Heap* heap,
                                                  char* obj,
                                                  char* key,
                                                  intptr_t insert);
intptr_t RuntimeLookupProperty(Heap* heap,
                               char* obj,
                               char* key,
                               intptr_t insert);

typedef char* (*RuntimeGrowObjectCallback)(Heap* heap,
                                           char* obj,
                                           uint32_t min_size);
char* RuntimeGrowObject(Heap* heap, char* obj, uint32_t min_size);

typedef char* (*RuntimeCoerceCallback)(Heap* heap, char* value);
char* RuntimeToString(Heap* heap, char* value);
char* RuntimeToNumber(Heap* heap, char* value);
char* RuntimeToBoolean(Heap* heap, char* value);

typedef size_t (*RuntimeCompareCallback)(Heap* heap, char* lhs, char* rhs);
size_t RuntimeStrictCompare(Heap* heap, char* lhs, char* rhs);
size_t RuntimeStringCompare(Heap* heap, char* lhs, char* rhs);

char* RuntimeConcatenateStrings(Heap* heap, char* lhs, char* rhs);

Heap::HeapTag RuntimeCoerceType(Heap* heap,
                                BinOp::BinOpType type,
                                char* &lhs,
                                char* &rhs);

typedef char* (*RuntimeBinOpCallback)(Heap* heap, char* lhs, char* rhs);
template <BinOp::BinOpType type>
char* RuntimeBinOp(Heap* heap, char* lhs, char* rhs);

typedef char* (*RuntimeSizeofCallback)(Heap* heap, char* value);
char* RuntimeSizeof(Heap* heap, char* value);

typedef char* (*RuntimeKeysofCallback)(Heap* heap, char* value);
char* RuntimeKeysof(Heap* heap, char* value);

char* RuntimeCloneObject(Heap* heap, char* obj);

typedef void (*RuntimeDeletePropertyCallback)(Heap* heap,
                                              char* obj,
                                              char* property);
void RuntimeDeleteProperty(Heap* heap, char* obj, char* property);

typedef char* (*RuntimeStackTraceCallback)(Heap* heap, char** frame, char* ip);
char* RuntimeStackTrace(Heap* heap, char** frame, char* ip);

} // namespace internal
} // namespace candor

#endif // _SRC_RUNTIME_H_
