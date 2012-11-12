#ifndef _SRC_FULLGEN_INL_H_
#define _SRC_FULLGEN_INL_H_

namespace candor {
namespace internal {

inline void Fullgen::Print(char* out, int32_t size) {
  PrintBuffer p(out, size);
  Print(&p);
}

} // namespace internal
} // namespace candor

#endif // _SRC_FULLGEN_INL_H_
