#include "runtime.h"
#include "heap.h" // Heap
#include "heap-inl.h"
#include "utils.h" // ComputeHash, etc

#define __STDC_FORMAT_MACROS
#include <inttypes.h> // printf formats for big integers
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
    V(Mod)\
    V(BAnd)\
    V(BOr)\
    V(BXor)\
    V(Shl)\
    V(Shr)\
    V(UShr)\
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


intptr_t RuntimeGetHash(Heap* heap, char* value) {
  Heap::HeapTag tag = HValue::GetTag(value);

  switch (tag) {
   case Heap::kTagString:
    return HString::Hash(heap, value);
   case Heap::kTagFunction:
   case Heap::kTagObject:
   case Heap::kTagArray:
   case Heap::kTagCData:
    return reinterpret_cast<intptr_t>(value);
   case Heap::kTagNil:
    return 0;
   case Heap::kTagBoolean:
    if (HBoolean::Value(value)) {
      return 1;
    } else {
      return 0;
    }
   case Heap::kTagNumber:
    {
      int64_t intval;

      if (HValue::IsUnboxed(value)) {
        intval = HNumber::IntegralValue(value);
      } else {
        intval = HNumber::DoubleValue(value);
      }

      // And create new string
      return ComputeHash(intval);
    }
   default:
    UNEXPECTED
  }

  return 0;
}


intptr_t RuntimeLookupProperty(Heap* heap,
                               char* obj,
                               char* key,
                               intptr_t insert) {
  assert(!HValue::Cast(obj)->IsGCMarked());
  assert(!HValue::Cast(obj)->IsSoftGCMarked());

  char* map = HObject::Map(obj);
  char* space = HValue::As<HMap>(map)->space();
  uint32_t mask = HObject::Mask(obj);

  bool is_array = HValue::GetTag(obj) == Heap::kTagArray;

  char* keyptr = NULL;
  int64_t numkey = 0;
  uint32_t hash = 0;

  if (is_array) {
    numkey = HNumber::IntegralValue(RuntimeToNumber(heap, key));
    keyptr = HNumber::ToPointer(numkey);
    hash = ComputeHash(numkey);

    // Negative lookups are prohibited
    if (numkey < 0) return Heap::kTagNil;

    // Update array's length on insertion (if increased)
    if (insert && HArray::Length(obj, false) <= numkey) {
      HArray::SetLength(obj, numkey + 1);
    }
  } else {
    assert(HValue::GetTag(obj) == Heap::kTagObject);
    keyptr = key;
    hash = RuntimeGetHash(heap, key);
  }

  if (is_array && HArray::IsDense(obj)) {
    // Dense arrays use another lookup mechanism
    uint32_t index = numkey * HValue::kPointerSize;

    if (index > mask) {
      if (insert) {
        RuntimeGrowObject(heap, obj, numkey);

        return RuntimeLookupProperty(heap, obj, keyptr, insert);
      } else {
        // get a[length + x] == nil
        return Heap::kTagNil;
      }
    }

    return HMap::kSpaceOffset + (index & mask);
  } else {
    // Dive into space and walk it in circular manner
    uint32_t start = hash & mask;

    uint32_t index = start;
    char* key_slot = NULL;
    bool needs_grow = true;
    do {
      key_slot = *reinterpret_cast<char**>(space + index);
      if (key_slot == HNil::New() ||
          (is_array && key_slot == keyptr) ||
          RuntimeStrictCompare(heap, key_slot, key) == 0) {
        needs_grow = false;
        break;
      }

      index += HValue::kPointerSize;
      index = index & mask;
    } while (index != start);

    if (insert) {
      // All key slots are filled - rehash and lookup again
      if (needs_grow) {
        RuntimeGrowObject(heap, obj, 0);

        return RuntimeLookupProperty(heap, obj, keyptr, insert);
      }

      if (key_slot == HNil::New()) {
        // Reset proto, IC could not work with this object anymore
        char** proto_slot = HObject::ProtoSlot(obj);
        *reinterpret_cast<intptr_t*>(proto_slot) = Heap::kICDisabledValue;
      }

      *reinterpret_cast<char**>(space + index) = keyptr;
    }

    return HMap::kSpaceOffset + index + (mask + HValue::kPointerSize);
  }
}


char* RuntimeGrowObject(Heap* heap, char* obj, uint32_t min_size) {
  char** map_addr = HObject::MapSlot(obj);
  HMap* map = HValue::As<HMap>(*map_addr);
  uint32_t size = map->size() << 1;

  if (min_size > size) {
    size = PowerOfTwo(min_size);
  }

  // Create a new map
  char* new_map = HMap::NewEmpty(heap, size);

  // Replace old map with a new
  *map_addr = new_map;

  // Update mask
  uint32_t mask = (size - 1) * HValue::kPointerSize;
  *HObject::MaskSlot(obj) = mask;

  // And rehash properties to new map
  uint32_t original_size = map->size();
  if (HValue::GetTag(obj) == Heap::kTagArray && HArray::IsDense(obj)) {
    // Dense array's map doesn't contain key pointers, iterate values
    original_size = original_size << 1;
    for (uint32_t i = 0; i < original_size; i++) {
      char* value = *map->GetSlotAddress(i);
      if (value == HNil::New()) continue;

      *HObject::LookupProperty(heap, obj, HNumber::ToPointer(i), 1) = value;
    }
  } else {
    // Object and non-dense arrays contains both keys and pointers
    for (uint32_t i = 0; i < original_size; i++) {
      char* key = *map->GetSlotAddress(i);
      if (key == HNil::New()) continue;

      char* value = *map->GetSlotAddress(i + original_size);

      *HObject::LookupProperty(heap, obj, key, 1) = value;
    }
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
        len = snprintf(str, sizeof(str), "%" PRIi64, num);
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

  return HNil::New();
}


char* RuntimeToNumber(Heap* heap, char* value) {
  Heap::HeapTag tag = HValue::GetTag(value);

  switch (tag) {
   case Heap::kTagString:
    {
      char* str = HString::Value(heap, value);
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

  return HNil::New();
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

  return HNil::New();
}


intptr_t RuntimeStrictCompare(Heap* heap, char* lhs, char* rhs) {
  // Fast case - pointers are equal
  if (lhs == rhs) return 0;

  Heap::HeapTag tag = HValue::GetTag(lhs);
  Heap::HeapTag rtag = HValue::GetTag(rhs);

  // We can only compare objects with equal type
  if (rtag != tag) return -1;

  switch (tag) {
   case Heap::kTagString:
    return RuntimeStringCompare(heap, lhs, rhs);
   case Heap::kTagFunction:
   case Heap::kTagObject:
   case Heap::kTagArray:
   case Heap::kTagCData:
   case Heap::kTagNil:
    return -1;
   case Heap::kTagBoolean:
    return HBoolean::Value(lhs) == HBoolean::Value(rhs) ? 0 : -1;
   case Heap::kTagNumber:
    return HNumber::DoubleValue(lhs) == HNumber::DoubleValue(rhs) ? 0 : -1;
   default:
    UNEXPECTED
  }

  return 0;
}


intptr_t RuntimeStringCompare(Heap* heap, char* lhs, char* rhs) {
  uint32_t lhs_length = HString::Length(lhs);
  uint32_t rhs_length = HString::Length(rhs);

  return lhs_length < rhs_length ? -1 :
         lhs_length > rhs_length ? 1 :
         strncmp(HString::Value(heap, lhs),
                 HString::Value(heap, rhs),
                 lhs_length);
}


char* RuntimeConcatenateStrings(Heap* heap,
                                char* lhs,
                                char* rhs) {
  int32_t lhs_length = HString::Length(lhs);
  int32_t rhs_length = HString::Length(rhs);

  char* result;
  if (lhs_length + rhs_length < HString::kMinConsLength) {
    result = HString::New(heap, Heap::kTenureNew, lhs_length + rhs_length);

    memcpy(HString::Value(heap, result),
           HString::Value(heap, lhs), lhs_length);
    memcpy(HString::Value(heap, result) + lhs_length,
           HString::Value(heap, rhs), rhs_length);
  } else {
    result = HString::NewCons(heap,
                              Heap::kTenureNew,
                              lhs_length + rhs_length,
                              lhs,
                              rhs);
  }

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
    return RuntimeCoerceType(heap, type, rhs, lhs);
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
  if (lhs == HNil::New() && rhs == HNil::New()) {
    if (BinOp::is_math(type) || BinOp::is_binary(type)) {
      // nil (+) nil = 0
      return HNumber::New(heap, 0);
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
        result = RuntimeStringCompare(heap, lhs, rhs) == 0;
      } else {
        result = BinOp::NumToCompare(type,
                                     RuntimeStringCompare(heap, lhs, rhs));
      }
      break;
     case Heap::kTagObject:
     case Heap::kTagArray:
     case Heap::kTagCData:
      // object (+) object = false
      if (BinOp::is_strict_eq(type)) {
        result = lhs == rhs;
      } else {
        result = false;
      }
      break;
     case Heap::kTagNil:
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

    return HBoolean::New(heap, Heap::kTenureNew, result);
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
             (HValue::GetTag(lhs) == Heap::kTagString ||
              HValue::GetTag(rhs) == Heap::kTagString)) {
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
       case BinOp::kMod: result = lval % rval; break;
       case BinOp::kShl: result = lval << rval; break;
       case BinOp::kShr: result = lval >> rval; break;
       case BinOp::kUShr:
        {
          intptr_t ulval = lval;
          intptr_t urval = rval;
          result = type == ulval >> urval;
        }
        break;
       default: UNEXPECTED
      }

      return HNumber::New(heap, Heap::kTenureNew, result);
    } else {
      UNEXPECTED
    }
  }

  return HNil::New();
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
    size = HArray::Length(value, true);
    break;
   case Heap::kTagFunction:
    size = HFunction::Argc(value);
    break;
   case Heap::kTagObject:
   case Heap::kTagNil:
   case Heap::kTagBoolean:
   case Heap::kTagNumber:
    size = 0;
    break;
   default:
    UNEXPECTED
  }
  return HNumber::New(heap, size);
}


char* RuntimeKeysof(Heap* heap, char* value) {
  Heap::HeapTag tag = HValue::GetTag(value);

  char* result = HArray::NewEmpty(heap);

  // Fast-case - return empty array
  if (tag != Heap::kTagArray && tag != Heap::kTagObject) return result;

  // Slow-case visit all map's slots and put them into array
  HMap* map = HValue::As<HMap>(HObject::Map(value));

  uint32_t size = map->size();
  uint32_t index = 0;
  for (uint32_t i = 0; i < size; i++) {
    if (map->GetSlot(i) != HValue::Cast(HNil::New())) {
      char** slot = HObject::LookupProperty(heap,
                                            result,
                                            HNumber::ToPointer(index),
                                            1);
      *slot = map->GetSlot(i)->addr();
      index++;
    }
  }

  return result;
}


char* RuntimeCloneObject(Heap* heap, char* obj) {
  Heap::HeapTag tag = HValue::GetTag(obj);
  if (tag != Heap::kTagObject && tag != Heap::kTagArray) return HNil::New();

  HObject* source_obj = HValue::As<HObject>(obj);
  HMap* source_map = HValue::As<HMap>(source_obj->map());

  char* result = heap->AllocateTagged(Heap::kTagObject,
                                      Heap::kTenureNew,
                                      2 * HValue::kPointerSize);

  char* map = heap->AllocateTagged(
      Heap::kTagMap,
      Heap::kTenureNew,
      ((source_map->size() << 1) + 1) * HValue::kPointerSize);

  // Set mask
  *reinterpret_cast<intptr_t*>(result + HObject::kMaskOffset) =
      (source_map->size() - 1) * HValue::kPointerSize;

  // Set map
  *reinterpret_cast<char**>(result + HObject::kMapOffset) = map;

  // Set map's proto
  *reinterpret_cast<void**>(obj + HObject::kProtoOffset) = source_map;

  // Set map's size
  *reinterpret_cast<intptr_t*>(map + HMap::kSizeOffset) = source_map->size();

  // Nullify all map's slots (both keys and values)
  uint32_t size = (source_map->size() << 1) * HValue::kPointerSize;
  memcpy(map + HMap::kSpaceOffset, source_map->space(), size);

  return result;
}


void RuntimeDeleteProperty(Heap* heap, char* obj, char* property) {
  Heap::HeapTag tag = HValue::GetTag(obj);
  if (tag != Heap::kTagObject && tag != Heap::kTagArray) return;

  intptr_t offset = RuntimeLookupProperty(heap, obj, property, 0);

  // Reset proto, IC could not work with this object anymore
  char** proto_slot = HObject::ProtoSlot(obj);
  *reinterpret_cast<intptr_t*>(proto_slot) = Heap::kICDisabledValue;

  // Dense arrays doesn't have keys
  if (HValue::GetTag(obj) != Heap::kTagArray || !HArray::IsDense(obj)) {
    // Nil property
    intptr_t keyoffset = offset - HObject::Mask(obj) - HValue::kPointerSize;
    *reinterpret_cast<intptr_t*>(HObject::Map(obj) + keyoffset) = Heap::kTagNil;
  }

  // Nil value
  *reinterpret_cast<intptr_t*>(HObject::Map(obj) + offset) = Heap::kTagNil;
}


char* RuntimeStackTrace(Heap* heap, char** frame, char* ip) {
  SourceInfo* info;
  char* result = HArray::NewEmpty(heap);

  char* file_sym = HString::New(heap, Heap::kTenureNew, "filename", 8);
  char* line_sym  = HString::New(heap, Heap::kTenureNew, "line", 4);
  char* off_sym  = HString::New(heap, Heap::kTenureNew, "offset", 6);

  uint32_t index = 0;
  while ((info = heap->source_map()->Get(ip)) != NULL) {
    if (ip != NULL) {
      char** slot;

      // Create object with info
      char* obj = HObject::NewEmpty(heap);

      // Put filename
      slot = HObject::LookupProperty(heap, obj, file_sym, 1);
      *slot = HString::New(heap,
                           Heap::kTenureNew,
                           info->filename(),
                           strlen(info->filename()));

      // Put line number and offset
      int pos;
      int line = GetSourceLineByOffset(info->source(), info->offset(), &pos);

      slot = HObject::LookupProperty(heap, obj, line_sym, 1);
      *slot = HNumber::New(heap, line);

      slot = HObject::LookupProperty(heap, obj, off_sym, 1);
      *slot = HNumber::New(heap, pos);

      // And put it in array
      slot = HObject::LookupProperty(heap,
                                     result,
                                     HNumber::ToPointer(index),
                                     1);
      *slot = obj;
      index++;
    }

    // Traverse stack
    if (frame == NULL) break;

    // Get return address and previous frame
    ip = *(frame + 1);
    frame = reinterpret_cast<char**>(*frame);

    // Detect frame enter
    if (frame != NULL &&
        static_cast<uint32_t>(reinterpret_cast<intptr_t>(*(frame + 2))) ==
            Heap::kEnterFrameTag) {
      frame = reinterpret_cast<char**>(*(frame + 4));
    }
  }

  return result;
}


void RuntimeLoadPropertyPIC(char* ip) {
  // TODO: Implement me
}

} // namespace internal
} // namespace candor
