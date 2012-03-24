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

  void Push(const uint32_t jit_offset,  const uint32_t offset);
  void Commit(const char* source, uint32_t length, char* addr);
  SourceInfo* Get(char* addr);

  inline SourceQueue* queue() { return &queue_; }

 private:
  SourceQueue queue_;
};

class SourceInfo {
 public:
  SourceInfo(const uint32_t offset,
             const uint32_t jit_offset) : source_(NULL),
                                          length_(NULL),
                                          offset_(offset),
                                          jit_offset_(jit_offset) {
  }

  inline const char* source() { return source_; }
  inline uint32_t length() { return length_; }
  inline void source(const char* source) { source_ = source; }
  inline void length(uint32_t length) { length_ = length; }

  inline const uint32_t offset() { return offset_; }
  inline const uint32_t jit_offset() { return jit_offset_; }

 private:
  const char* source_;
  uint32_t length_;
  const uint32_t offset_;
  const uint32_t jit_offset_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_SOURCE_MAP_H_
