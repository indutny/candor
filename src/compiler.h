#ifndef _SRC_COMPILER_H_
#define _SRC_COMPILER_H_

#include "zone.h" // Zone

#include <stdint.h> // uint32_t
#include <string.h> // memcpy

namespace candor {

// Forward declaration
class Heap;


// Guards executable page with non-readable&non-executable page
class Guard {
 public:
  Guard(char* buffer, uint32_t length);
  ~Guard();

  typedef char* (*CompiledFunction)(void* context, uint32_t args, char* root);

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
  char* Run();

  bool CaughtException();

 private:
  Zone zone_;
  Heap* heap_;
  Guard* guard_;

  char* source_;
  uint32_t length_;

  char* root_context_;
};

} // namespace candor

#endif // _SRC_COMPILER_H_
