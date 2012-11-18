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

#include "source-map.h"

#include <stdlib.h>  // NULL
#include <unistd.h>  // intptr_t
#include <stdint.h>  // uint32_t

#include "utils.h"  // HashMap

namespace candor {
namespace internal {

void SourceMap::Push(const uint32_t jit_offset,
                     const uint32_t offset) {
  queue()->Push(new SourceInfo(offset, jit_offset));
}


void SourceMap::Commit(const char* filename,
                       const char* source,
                       uint32_t length,
                       char* addr) {
  intptr_t addr_o = reinterpret_cast<intptr_t>(addr);

  SourceInfo* info;
  while ((info = queue()->Shift()) != NULL) {
    info->filename(filename);
    info->source(source);
    info->length(length);

    SourceMapBase::Insert(NumberKey::New(addr_o + info->jit_offset()),
                          info);
  }
}


SourceInfo* SourceMap::Get(char* addr) {
  intptr_t addr_o = reinterpret_cast<intptr_t>(addr);

  return SourceMapBase::Find(NumberKey::New(addr_o));
}

}  // namespace internal
}  // namespace candor
