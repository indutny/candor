#include "source-map.h"
#include "utils.h" // HashMap

#include <stdlib.h> // NULL
#include <sys/types.h> // off_t

namespace candor {
namespace internal {

void SourceMap::Push(const uint32_t jit_offset,
                     const uint32_t offset) {
  queue()->Push(new SourceInfo(offset, jit_offset));
}


void SourceMap::Commit(const char* source, uint32_t length, char* addr) {
  off_t addr_o = reinterpret_cast<off_t>(addr);

  SourceInfo* info;
  while ((info = queue()->Shift()) != NULL) {
    info->source(source);
    info->length(length);

    SourceMapBase::Set(NumberKey::New(addr_o + info->jit_offset()),
                       info);
  }
}


SourceInfo* SourceMap::Get(char* addr) {
  off_t addr_o = reinterpret_cast<off_t>(addr);

  return SourceMapBase::Get(NumberKey::New(addr_o));
}

} // namespace internal
} // namespace candor
