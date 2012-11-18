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

#ifndef _SRC_ASSEMBLER_H_
#define _SRC_ASSEMBLER_H_

#if CANDOR_ARCH_x64
#include "x64/assembler-x64.h"
#include "x64/assembler-x64-inl.h"
#elif CANDOR_ARCH_ia32
#include "ia32/assembler-ia32.h"
#include "ia32/assembler-ia32-inl.h"
#endif

#include "zone.h"

namespace candor {
namespace internal {

class Label : public ZoneObject {
 public:
  Label() : pos_(0) {
  }

  inline void AddUse(Assembler* a, RelocationInfo* use);

 private:
  inline void relocate(uint32_t offset);
  inline void use(Assembler* a, uint32_t offset);

  uint32_t pos_;
  ZoneList<RelocationInfo*> uses_;

  friend class Assembler;
};

}  // namespace internal
}  // namespace candor

#endif  // _SRC_ASSEMBLER_H_
