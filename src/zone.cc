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

#include "zone.h"

#include <sys/types.h>  // size_t

#include "utils.h"  // RoundUp

namespace candor {
namespace internal {

Zone* Zone::current_ = NULL;

void* Zone::Allocate(size_t size) {
  // If current block has enough size - allocate chunk in it
  if (blocks_.head()->value()->has(size)) {
    return blocks_.head()->value()->allocate(size);
  } else {
    // Otherwise create new block that will definitely fit that chunk
    ZoneBlock* block = new ZoneBlock(RoundUp(size, page_size_));
    blocks_.Unshift(block);
    return block->allocate(size);
  }
}


void* ZonePolicy::Allocate(size_t size) {
  return Zone::current()->Allocate(size);
}

}  // namespace internal
}  // namespace candor
