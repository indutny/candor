#include "source-map.h"
#include "utils.h" // HashMap

#include <stdlib.h> // NULL
#include <unistd.h> // intptr_t
#include <stdint.h> // uint32_t

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

} // namespace internal
} // namespace candor
