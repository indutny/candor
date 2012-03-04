#include "runtime.h"
#include "heap.h" // Heap
#include "utils.h" // ComputeHash, etc

#include <stdint.h> // uint32_t
#include <assert.h> // assert
#include <string.h> // strncmp
#include <stdio.h> // snprintf
#include <sys/types.h> // size_t

namespace candor {

// Declare all template function variants
#define BINARY_SUB_TYPES(V)\
    V(Add)\
    V(Sub)\
    V(Mul)\
    V(Div)\
    V(BAnd)\
    V(BOr)\
    V(BXor)\
    V(Eq)\
    V(StrictEq)\
    V(Ne)\
    V(StrictNe)\
    V(Lt)\
    V(Gt)\
    V(Le)\
    V(Ge)\
    V(LOr)\
    V(LAnd)

#define BINARY_OP_TEMPLATE(V)\
    template char* RuntimeBinOp<BinOp::k##V>(Heap* heap,\
                                             char* stack_top,\
                                             char* lhs,\
                                             char* rhs);

BINARY_SUB_TYPES(BINARY_OP_TEMPLATE)

#undef BINARY_OP_TEMPLATE
#undef BINARY_SUB_TYPES

char* RuntimeAllocate(Heap* heap,
                      uint32_t bytes,
                      char* stack_top) {
  return heap->new_space()->Allocate(bytes, stack_top);
}


void RuntimeCollectGarbage(Heap* heap, char* stack_top) {
  Zone gc_zone;
  heap->gc()->CollectGarbage(stack_top);
}


char* RuntimeLookupProperty(Heap* heap,
                            char* stack_top,
                            char* obj,
                            char* key,
                            off_t insert) {
  char* map = *reinterpret_cast<char**>(obj + 16);
  char* space = map + 16;
  uint32_t mask = *reinterpret_cast<uint64_t*>(obj + 8);

  char* strkey = RuntimeToString(heap, stack_top, key);

  // Compute hash lazily
  uint32_t* hash_addr = reinterpret_cast<uint32_t*>(strkey + 8);
  uint32_t hash = *hash_addr;
  if (hash == 0) {
    hash = ComputeHash(strkey + 24, *reinterpret_cast<uint32_t*>(strkey + 16));
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
    if (RuntimeCompare(key_slot, strkey) == 0) break;

    index += 8;
    if (index > mask) index = 0;
  }

  // All key slots are filled - rehash and lookup again
  if (index == end) {
    assert(insert);
    RuntimeGrowObject(heap, stack_top, obj);
    return RuntimeLookupProperty(heap, stack_top, obj, strkey, insert);
  }

  if (insert) {
    *reinterpret_cast<char**>(space + index) = strkey;
  }

  return space + index + (mask + 8);
}


char* RuntimeGrowObject(Heap* heap, char* stack_top, char* obj) {
  char** map_addr = reinterpret_cast<char**>(obj + 16);
  char* map = *map_addr;
  uint32_t size = *reinterpret_cast<uint32_t*>(map + 8);

  char* new_map = heap->AllocateTagged(Heap::kTagMap,
                                       8 + (size << 5),
                                       stack_top);
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
    char* key = *reinterpret_cast<char**>(space + index);
    if (key== NULL) continue;

    char* value = *reinterpret_cast<char**>(space + index + (size << 3));
    assert(value != NULL);

    char* slot = RuntimeLookupProperty(heap, stack_top, obj, key, true);
    *reinterpret_cast<char**>(slot) = value;
  }

  return 0;
}


char* RuntimeToString(Heap* heap, char* stack_top, char* value) {
  Heap::HeapTag tag = HValue::GetTag(value);

  switch (tag) {
   case Heap::kTagString:
    return value;
   case Heap::kTagFunction:
   case Heap::kTagObject:
   case Heap::kTagNil:
    return HString::New(heap, stack_top, "", 0);
   case Heap::kTagBoolean:
    if (HBoolean::Value(value)) {
      return HString::New(heap, stack_top, "true", 4);
    } else {
      return HString::New(heap, stack_top, "false", 5);
    }
   case Heap::kTagNumber:
    {
      char str[128];
      uint32_t len;

      if (HValue::IsUnboxed(value)) {
        int64_t num = HNumber::Untag(reinterpret_cast<int64_t>(value));
        // Maximum int64 value may contain only 20 chars, last one for '\0'
        len = snprintf(str, sizeof(str), "%lld", num);
      } else {
        double num = HNumber::DoubleValue(value);
        len = snprintf(str, sizeof(str), "%g", num);
      }

      // And create new string
      return HString::New(heap, stack_top, str, len);
    }
   default:
    assert(0 && "Unexpected");
  }

  return NULL;
}


char* RuntimeToNumber(Heap* heap, char* stack_top, char* value) {
  Heap::HeapTag tag = HValue::GetTag(value);

  switch (tag) {
   case Heap::kTagString:
    {
      char* str = value + 24;
      uint32_t length = HString::Length(value);

      return HNumber::New(heap, stack_top, StringToInt(str, length));
    }
   case Heap::kTagBoolean:
    {
      uint64_t val = HBoolean::Value(value) ? 1 : 0;
      return HNumber::New(heap, stack_top, val);
    }
   case Heap::kTagFunction:
   case Heap::kTagObject:
   case Heap::kTagNil:
    return HNumber::New(heap, stack_top, static_cast<uint64_t>(0));
   case Heap::kTagNumber:
    return value;
   default:
    assert(0 && "Unexpected");
  }

  return NULL;
}


char* RuntimeToBoolean(Heap* heap, char* stack_top, char* value) {
  Heap::HeapTag tag = HValue::GetTag(value);

  switch (tag) {
   case Heap::kTagString:
    return HBoolean::New(heap, stack_top, HString::Length(value) > 0);
   case Heap::kTagBoolean:
    return value;
   case Heap::kTagFunction:
   case Heap::kTagObject:
    return HBoolean::New(heap, stack_top, true);
   case Heap::kTagNil:
    return HBoolean::New(heap, stack_top, false);
   case Heap::kTagNumber:
    if (HValue::IsUnboxed(value)) {
      int64_t num = HNumber::Untag(reinterpret_cast<int64_t>(value));
      return HBoolean::New(heap, stack_top, num != 0);
    } else {
      double num = HNumber::DoubleValue(value);
      return HBoolean::New(heap, stack_top, num != 0);
    }
   default:
    assert(0 && "Unexpected");
  }

  return NULL;
}


size_t RuntimeCompare(char* lhs, char* rhs) {
  Heap::HeapTag lhs_tag = HValue::GetTag(lhs);
  Heap::HeapTag rhs_tag = HValue::GetTag(rhs);

  if (lhs_tag != Heap::kTagString) assert(0 && "Not implemented yet");
  if (rhs_tag != Heap::kTagString) assert(0 && "Not implemented yet");

  off_t lhs_length = HString::Length(lhs);
  off_t rhs_length = HString::Length(rhs);

  return lhs_length < rhs_length ? -1 :
         lhs_length > rhs_length ? 1 :
         strncmp(lhs + 24, rhs + 24, lhs_length);
}


void RuntimeCoerceType(Heap* heap,
                        char* stack_top,
                        BinOp::BinOpType type,
                        char* &lhs,
                        char* &rhs) {
}


template <BinOp::BinOpType type>
char* RuntimeBinOp(Heap* heap, char* stack_top, char* lhs, char* rhs) {
  // Fast case: both sides are nil
  if (lhs == NULL && rhs == NULL) {
    if (BinOp::is_math(type) || BinOp::is_binary(type)) {
      // nil (+) nil = 0
      return HNumber::New(heap, stack_top, static_cast<uint64_t>(0));
    } else if (BinOp::is_logic(type) || BinOp::is_bool_logic(type)) {
      // nil == nil = true
      // nil === nil = true
      // nil (+) nil = false
      return HBoolean::New(heap,
                           stack_top,
                           !BinOp::is_negative_eq(type));
    }
  }

  // Equality operations
  if (BinOp::is_equality(type)) {
    // nil == expr, expr == nil
    if (lhs == NULL || rhs == NULL) {
      return HBoolean::New(heap, stack_top, false);
    }

    Heap::HeapTag lhs_tag = HValue::GetTag(lhs);
    if (BinOp::is_strict_eq(type)) {
      Heap::HeapTag rhs_tag = HValue::GetTag(rhs);

      // When strictly comparing - tags should be equal
      if (lhs_tag != rhs_tag) {
        return HBoolean::New(heap, stack_top, BinOp::is_negative_eq(type));
      }
    } else {
      RuntimeCoerceType(heap, stack_top, type, lhs, rhs);
    }

    switch (lhs_tag) {
     case Heap::kTagString:
      // Compare strings
      abort();
      break;
     case Heap::kTagObject:
      // object != object
      return HBoolean::New(heap, stack_top, false);
    }
  }

  return NULL;
}

} // namespace candor
