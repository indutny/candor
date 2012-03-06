#ifndef _SRC_HEAP_INL_H_
#define _SRC_HEAP_INL_H_

namespace candor {
namespace internal {

inline Heap::HeapTag HValue::GetTag(char* addr) {
  if (addr == NULL) return Heap::kTagNil;

  if (IsUnboxed(addr)) {
    return Heap::kTagNumber;
  }
  return static_cast<Heap::HeapTag>(*reinterpret_cast<uint8_t*>(addr));
}


inline bool HValue::IsUnboxed(char* addr) {
  return (reinterpret_cast<off_t>(addr) & 0x01) == 0x01;
}


inline bool HValue::IsGCMarked() {
  if (IsUnboxed(addr())) return false;
  return (*reinterpret_cast<uint64_t*>(addr()) & 0x80000000) == 0x80000000;
}


inline char* HValue::GetGCMark() {
  assert(IsGCMarked());
  return *reinterpret_cast<char**>(addr() + 8);
}


inline void HValue::SetGCMark(char* new_addr) {
  *reinterpret_cast<uint64_t*>(addr()) |= 0x80000000;
  *reinterpret_cast<char**>(addr() + 8) = new_addr;
}


inline void HValue::ResetGCMark() {
  if (IsGCMarked()) {
    *reinterpret_cast<uint64_t*>(addr()) ^= 0x80000000;
  }
}


inline int64_t HNumber::Untag(int64_t value) {
  return value >> 1;
}


inline int64_t HNumber::Tag(int64_t value) {
  return (value << 1) | 1;
}


inline int64_t HNumber::IntegralValue(char* addr) {
  if (IsUnboxed(addr)) {
    return Untag(reinterpret_cast<int64_t>(addr));
  } else {
    return *reinterpret_cast<double*>(addr + 8);
  }
}


inline double HNumber::DoubleValue(char* addr) {
  if (IsUnboxed(addr)) {
    return Untag(reinterpret_cast<int64_t>(addr));
  } else {
    return *reinterpret_cast<double*>(addr + 8);
  }
}

} // namespace internal
} // namespace candor

#endif // _SRC_HEAP_INL_H_
