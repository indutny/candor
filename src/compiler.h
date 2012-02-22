#ifndef _SRC_COMPILER_H_
#define _SRC_COMPILER_H_

#include <stdint.h> // uint32_t
#include <string.h> // memcpy

namespace dotlang {

class Heap;
typedef void* (*CompiledFunction)(Heap* heap);

// Guards executable page with non-readable&non-executable page
class Guard {
 public:
  Guard(char* buffer, uint32_t length);
  ~Guard();

  inline CompiledFunction AsFunction() {
    return reinterpret_cast<CompiledFunction>(buffer_);
  }

  inline char* buffer() { return reinterpret_cast<char*>(buffer_); }

 private:
  void* buffer_;
  void* guard_;
  uint32_t length_;
  uint32_t page_size_;
};

// Main compilator object holds source, heap, and compiled code
class CompiledScript {
 public:
  CompiledScript(const char* source, uint32_t length);

  ~CompiledScript();

  void Compile();
  void* Run();

 private:
  Heap* heap_;
  Guard* guard_;

  char* source_;
  uint32_t length_;
};

} // namespace dotlang

#endif // _SRC_COMPILER_H_
