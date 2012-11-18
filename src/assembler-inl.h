/**
 * Copyright (c) 2012, Fedor Indutny.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _SRC_ASSEMBLER_INL_H_
#define _SRC_ASSEMBLER_INL_H_

namespace candor {
namespace internal {

inline void Label::AddUse(Assembler* a, RelocationInfo* use) {
  // If we already know target position - set it
  if (pos_ != 0) use->target(pos_);
  uses_.Push(use);
  a->relocation_info_.Push(use);
}


inline void Label::relocate(uint32_t offset) {
  // Label should be relocated only once
  assert(pos_ == 0);
  pos_ = offset;

  // Iterate through all label's uses and insert correct relocation info
  ZoneList<RelocationInfo*>::Item* item = uses_.head();
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
  AddUse(a, info);
}

}  // namespace internal
}  // namespace candor

#endif  // _SRC_ASSEMBLER_INL_H_
