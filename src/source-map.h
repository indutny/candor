#ifndef _SRC_SOURCE_MAP_H_
#define _SRC_SOURCE_MAP_H_

#include "utils.h" // HashMap

namespace candor {
namespace internal {

// Forward declaration
class SourceInfo;

typedef HashMap<NumberKey, SourceInfo, EmptyClass> SourceMapBase;

class SourceMap : SourceMapBase {
 public:
  typedef List<SourceInfo*, EmptyClass> SourceQueue;

  SourceMap() {
    // SourceInfo should be 'delete'ed on destruction
    allocated = true;
    queue_.allocated = true;
  }

  void Push(const char* source,
            const uint32_t length,
            const uint32_t offset,
            const uint32_t jit_offset);
  void Commit(char* addr);
  SourceInfo* Get(char* addr);

  inline SourceQueue* queue() { return &queue_; }

 private:
  SourceQueue queue_;
};

class SourceInfo {
 public:
  SourceInfo(const char* source,
             const uint32_t length,
             const uint32_t offset,
             const uint32_t jit_offset) : source_(source),
                                          length_(length),
                                          offset_(offset),
                                          jit_offset_(jit_offset) {
  }

  inline const char* source() { return source_; }
  inline const uint32_t length() { return length_; }
  inline const uint32_t offset() { return offset_; }
  inline const uint32_t jit_offset() { return jit_offset_; }

 private:
  const char* source_;
  const uint32_t length_;
  const uint32_t offset_;
  const uint32_t jit_offset_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_SOURCE_MAP_H_
