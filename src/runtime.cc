#include "runtime.h"
#include "heap.h" // Heap
#include "heap-inl.h"
#include "utils.h" // ComputeHash, etc

#include <stdint.h> // uint32_t
#include <assert.h> // assert
#include <string.h> // strncmp
#include <stdio.h> // snprintf
#include <sys/types.h> // size_t

namespace candor {
namespace internal {

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
                                             char* lhs,\
                                             char* rhs);

BINARY_SUB_TYPES(BINARY_OP_TEMPLATE)

#undef BINARY_OP_TEMPLATE
#undef BINARY_SUB_TYPES

char* RuntimeAllocate(Heap* heap,
                      uint32_t bytes) {
  return heap->new_space()->Allocate(bytes);
}


void RuntimeCollectGarbage(Heap* heap, char* stack_top) {
  Zone gc_zone;
  heap->gc()->CollectGarbage(stack_top);
}


char* RuntimeLookupProperty(Heap* heap,
                            char* obj,
                            char* key,
                            off_t insert) {
  char* map = HObject::Map(obj);
  char* space = HValue::As<HMap>(map)->space();
  uint32_t mask = HObject::Mask(obj);

  bool is_array = HValue::GetTag(obj) == Heap::kTagArray;

  char* strkey;
  int64_t numkey;
  uint32_t hash;

  if (is_array) {
    numkey = HNumber::IntegralValue(RuntimeToNumber(heap, key));
    strkey = reinterpret_cast<char*>(HNumber::Tag(numkey));
    hash = ComputeHash(numkey);

    // Update array's length on insertion (if increased)
    if (insert && numkey > 0 && HArray::Length(obj) <= numkey) {
      HArray::SetLength(obj, numkey + 1);
    }
  } else {
    strkey = RuntimeToString(heap, key);
    hash = HString::Hash(strkey);
  }

  // Dive into space and walk it in circular manner
  uint32_t start = hash & mask;
  uint32_t end = start == 0 ? mask : start - 8;

  uint32_t index = start;
  char* key_slot = NULL;
  while (index != end) {
    key_slot = *reinterpret_cast<char**>(space + index);
    if (key_slot == NULL) break;
    if (is_array) {
      if (key_slot == strkey) break;
    } else {
      if (RuntimeStringCompare(key_slot, strkey) == 0) break;
    }

    index += 8;
    if (index > mask) index = 0;
  }

  // All key slots are filled - rehash and lookup again
  if (index == end) {
    assert(insert);
    RuntimeGrowObject(heap, obj);

    return RuntimeLookupProperty(heap, obj, strkey, insert);
  }

  if (insert) {
    *reinterpret_cast<char**>(space + index) = strkey;
  }

  return space + index + (mask + 8);
}


char* RuntimeGrowObject(Heap* heap, char* obj) {
  char** map_addr = HObject::MapSlot(obj);
  char* map = *map_addr;
  uint32_t size = HValue::As<HMap>(map)->size();

  char* new_map = heap->AllocateTagged(Heap::kTagMap,
                                       Heap::kTenureNew,
                                       8 + (size << 5));
  // Set map size
  *(uint32_t*)(new_map + 8) = size << 1;

  // Fill new map with zeroes
  memset(new_map + 16, 0, size << 5);

  // Replace old map with a new
  *map_addr = new_map;

  // Change mask
  uint32_t mask = (size << 4) - 8;
  *HObject::MaskSlot(obj) = mask;

  char* space = HValue::As<HMap>(map)->space();

  // And rehash properties to new map
  for (uint32_t index = 0; index < size << 3; index += 8) {
    char* key = *reinterpret_cast<char**>(space + index);
    if (key== NULL) continue;

    char* value = *reinterpret_cast<char**>(space + index + (size << 3));
    assert(value != NULL);

    char* slot = RuntimeLookupProperty(heap, obj, key, true);
    *reinterpret_cast<char**>(slot) = value;
  }

  return 0;
}


char* RuntimeToString(Heap* heap, char* value) {
  Heap::HeapTag tag = HValue::GetTag(value);

  switch (tag) {
   case Heap::kTagString:
    return value;
   case Heap::kTagFunction:
   case Heap::kTagObject:
   case Heap::kTagArray:
   case Heap::kTagCData:
   case Heap::kTagNil:
    return HString::New(heap, Heap::kTenureNew, "", 0);
   case Heap::kTagBoolean:
    if (HBoolean::Value(value)) {
      return HString::New(heap, Heap::kTenureNew, "true", 4);
    } else {
      return HString::New(heap, Heap::kTenureNew, "false", 5);
    }
   case Heap::kTagNumber:
    {
      char str[128];
      uint32_t len;

      if (HValue::IsUnboxed(value)) {
        int64_t num = HNumber::IntegralValue(value);
        // Maximum int64 value may contain only 20 chars, last one for '\0'
        len = snprintf(str, sizeof(str), "%lld", num);
      } else {
        double num = HNumber::DoubleValue(value);
        len = snprintf(str, sizeof(str), "%g", num);
      }

      // And create new string
      return HString::New(heap, Heap::kTenureNew, str, len);
    }
   default:
    UNEXPECTED
  }

  return NULL;
}


char* RuntimeToNumber(Heap* heap, char* value) {
  Heap::HeapTag tag = HValue::GetTag(value);

  switch (tag) {
   case Heap::kTagString:
    {
      char* str = value + 24;
      uint32_t length = HString::Length(value);

      return HNumber::New(heap, Heap::kTenureNew, StringToDouble(str, length));
    }
   case Heap::kTagBoolean:
    {
      int64_t val = HBoolean::Value(value) ? 1 : 0;
      return HNumber::New(heap, Heap::kTenureNew, val);
    }
   case Heap::kTagFunction:
   case Heap::kTagObject:
   case Heap::kTagArray:
   case Heap::kTagCData:
   case Heap::kTagNil:
    return HNumber::New(heap, Heap::kTenureNew, static_cast<int64_t>(0));
   case Heap::kTagNumber:
    return value;
   default:
    UNEXPECTED
  }

  return NULL;
}


char* RuntimeToBoolean(Heap* heap, char* value) {
  Heap::HeapTag tag = HValue::GetTag(value);

  switch (tag) {
   case Heap::kTagString:
    return HBoolean::New(heap, Heap::kTenureNew, HString::Length(value) > 0);
   case Heap::kTagBoolean:
    return value;
   case Heap::kTagFunction:
   case Heap::kTagObject:
   case Heap::kTagArray:
   case Heap::kTagCData:
    return HBoolean::New(heap, Heap::kTenureNew, true);
   case Heap::kTagNil:
    return HBoolean::New(heap, Heap::kTenureNew, false);
   case Heap::kTagNumber:
    if (HValue::IsUnboxed(value)) {
      int64_t num = HNumber::IntegralValue(value);
      return HBoolean::New(heap, Heap::kTenureNew, num != 0);
    } else {
      double num = HNumber::DoubleValue(value);
      return HBoolean::New(heap, Heap::kTenureNew, num != 0);
    }
   default:
    UNEXPECTED
  }

  return NULL;
}


size_t RuntimeStringCompare(char* lhs, char* rhs) {
  uint32_t lhs_length = HString::Length(lhs);
  uint32_t rhs_length = HString::Length(rhs);

  return lhs_length < rhs_length ? -1 :
         lhs_length > rhs_length ? 1 :
         strncmp(HString::Value(lhs), HString::Value(rhs), lhs_length);
}


char* RuntimeConcatenateStrings(Heap* heap,
                                char* lhs,
                                char* rhs) {
  uint32_t lhs_length = HString::Length(lhs);
  uint32_t rhs_length = HString::Length(rhs);
  char* result = HString::New(heap, Heap::kTenureNew, lhs_length + rhs_length);

  memcpy(HString::Value(result), HString::Value(lhs), lhs_length);
  memcpy(HString::Value(result) + lhs_length, HString::Value(rhs), rhs_length);

  return result;
}


Heap::HeapTag RuntimeCoerceType(Heap* heap,
                                BinOp::BinOpType type,
                                char* &lhs,
                                char* &rhs) {
  Heap::HeapTag lhs_tag = HValue::GetTag(lhs);
  Heap::HeapTag rhs_tag = HValue::GetTag(rhs);

  // Fast case, values have same type
  if (lhs_tag == rhs_tag) return lhs_tag;

  switch (lhs_tag) {
   case Heap::kTagString:
    rhs = RuntimeToString(heap, rhs);
    break;
   case Heap::kTagBoolean:
    rhs = RuntimeToBoolean(heap, rhs);
    break;
   case Heap::kTagNil:
    rhs = NULL;
    break;
   case Heap::kTagFunction:
   case Heap::kTagObject:
   case Heap::kTagArray:
   case Heap::kTagCData:
    if (!BinOp::is_math(type) && !BinOp::is_binary(type)) {
      lhs = RuntimeToString(heap, lhs);
      rhs = RuntimeToString(heap, rhs);
      break;
    }
    lhs = RuntimeToNumber(heap, lhs);
   case Heap::kTagNumber:
    rhs = RuntimeToNumber(heap, rhs);
    break;
   default:
    UNEXPECTED
  }

  return lhs_tag;
}


template <BinOp::BinOpType type>
char* RuntimeBinOp(Heap* heap, char* lhs, char* rhs) {
  // Fast case: both sides are nil
  if (lhs == NULL && rhs == NULL) {
    if (BinOp::is_math(type) || BinOp::is_binary(type)) {
      // nil (+) nil = 0
      return HNumber::New(heap, static_cast<int64_t>(0));
    } else if (BinOp::is_logic(type) || BinOp::is_bool_logic(type)) {
      // nil == nil = true
      // nil === nil = true
      // nil (+) nil = false
      return HBoolean::New(heap,
                           Heap::kTenureNew,
                           !BinOp::is_negative_eq(type));
    }
  }

  // Logical operations ( <, >, >=, <=, ==, ===, !=, !== )
  if (BinOp::is_logic(type)) {
    Heap::HeapTag lhs_tag;

    // nil == expr, expr == nil
    if (BinOp::is_strict_eq(type)) {
      lhs_tag = HValue::GetTag(lhs);
      Heap::HeapTag rhs_tag = HValue::GetTag(rhs);

      // When strictly comparing - tags should be equal
      if (lhs_tag != rhs_tag) {
        return HBoolean::New(heap,
                             Heap::kTenureNew,
                             BinOp::is_negative_eq(type));
      }
    } else {
      lhs_tag = RuntimeCoerceType(heap, type, lhs, rhs);
    }

    bool result = false;
    switch (lhs_tag) {
     case Heap::kTagString:
      // Compare strings
      if (BinOp::is_equality(type)) {
        result = RuntimeStringCompare(lhs, rhs) == 0;
      } else {
        result = BinOp::NumToCompare(type, RuntimeStringCompare(lhs, rhs));
      }
      break;
     case Heap::kTagObject:
     case Heap::kTagArray:
     case Heap::kTagCData:
      // object (+) object = false
      result = false;
      break;
     case Heap::kTagFunction:
      if (BinOp::is_equality(type)) {
        result = lhs == rhs;
      } else {
        // Can't compare functions
        result = false;
      }
      break;
     case Heap::kTagNumber:
      if (HValue::IsUnboxed(lhs)) {
        int64_t lnum = HNumber::IntegralValue(lhs);
        int64_t rnum = HNumber::IntegralValue(rhs);

        if (BinOp::is_equality(type)) {
          result = lnum == rnum;
        } else {
          result = BinOp::NumToCompare(type, lnum - rnum);
        }
      } else {
        double lnum = HNumber::DoubleValue(lhs);
        double rnum = HNumber::DoubleValue(rhs);

        if (BinOp::is_equality(type)) {
          result = lnum == rnum;
        } else {
          result = BinOp::NumToCompare(type, lnum - rnum);
        }
      }
      break;
     case Heap::kTagBoolean:
      if (BinOp::is_equality(type)) {
        result = HBoolean::Value(lhs) == HBoolean::Value(rhs);
      } else {
        result = BinOp::NumToCompare(
            type, HBoolean::Value(lhs) - HBoolean::Value(rhs));
      }
      break;
     default:
      UNEXPECTED
      break;
    }

    if (BinOp::is_negative_eq(type)) result = !result;

    return HBoolean::New(heap, Heap::kTenureOld, result);
  } else if (BinOp::is_bool_logic(type)) {
    lhs = RuntimeToBoolean(heap, lhs);
    rhs = RuntimeToBoolean(heap, rhs);

    bool result = false;
    if (type == BinOp::kLAnd) {
      result = HBoolean::Value(lhs) && HBoolean::Value(rhs);
    } else if (type == BinOp::kLOr) {
      result = HBoolean::Value(lhs) || HBoolean::Value(rhs);
    } else {
      UNEXPECTED
    }

    return HBoolean::New(heap, Heap::kTenureNew, result);
  } else if (type == BinOp::kAdd &&
             HValue::GetTag(lhs) != Heap::kTagNumber &&
             HValue::GetTag(lhs) != Heap::kTagNil) {
    // String concatenation
    lhs = RuntimeToString(heap, lhs);
    rhs = RuntimeToString(heap, rhs);

    return RuntimeConcatenateStrings(heap, lhs, rhs);
  } else {
    lhs = RuntimeToNumber(heap, lhs);
    rhs = RuntimeToNumber(heap, rhs);

    if (BinOp::is_math(type)) {
      double result = 0;

      double lval = HNumber::DoubleValue(lhs);
      double rval = HNumber::DoubleValue(rhs);

      switch (type) {
       case BinOp::kAdd: result = lval + rval; break;
       case BinOp::kSub: result = lval - rval; break;
       case BinOp::kMul: result = lval * rval; break;
       case BinOp::kDiv: result = lval / rval; break;
       default: UNEXPECTED
      }

      return HNumber::New(heap, Heap::kTenureNew, result);
    } else if (BinOp::is_binary(type)) {
      int64_t result = 0;

      int64_t lval = HNumber::IntegralValue(lhs);
      int64_t rval = HNumber::IntegralValue(rhs);

      switch (type) {
       case BinOp::kBAnd: result = lval & rval; break;
       case BinOp::kBOr: result = lval | rval; break;
       case BinOp::kBXor: result = lval ^ rval; break;
       default: UNEXPECTED
      }

      return HNumber::New(heap, Heap::kTenureNew, result);
    } else {
      UNEXPECTED
    }
  }

  return NULL;
}


char* RuntimeSizeof(Heap* heap, char* value) {
  Heap::HeapTag tag = HValue::GetTag(value);

  int64_t size = 0;
  switch (tag) {
   case Heap::kTagString:
    size = HString::Length(value);
    break;
   case Heap::kTagCData:
    size = HCData::Size(value);
    break;
   case Heap::kTagArray:
    size = HArray::Length(value);
    break;
   case Heap::kTagObject:
   case Heap::kTagNil:
   case Heap::kTagFunction:
   case Heap::kTagBoolean:
   case Heap::kTagNumber:
    size = 0;
    break;
   default:
    UNEXPECTED
  }
  return HNumber::New(heap, size);
}

} // namespace internal
} // namespace candor
