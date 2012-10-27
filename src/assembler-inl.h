#ifndef _SRC_ASSEMBLER_INL_H_
#define _SRC_ASSEMBLER_INL_H_

namespace candor {
namespace internal {

inline void Label::relocate(uint32_t offset) {
  // Label should be relocated only once
  assert(pos_ == 0);
  pos_ = offset;

  // Iterate through all label's uses and insert correct relocation info
  List<RelocationInfo*, EmptyClass>::Item* item = uses_.head();
  while (item != NULL) {
    item->value()->target(pos_);
    item = item->next();
  }
}


inline void Label::use(Assembler* a, uint32_t offset) {
  RelocationInfo* info = new RelocationInfo(
      RelocationInfo::kRelative,
      RelocationInfo::kLong,
      offset);
  // If we already know target position - set it
  if (pos_ != 0) info->target(pos_);
  uses_.Push(info);
  a->relocation_info_.Push(info);
}

} // namespace internal
} // namespace candor

#endif // _SRC_ASSEMBLER_INL_H_
