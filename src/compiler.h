#ifndef _SRC_COMPILER_H_
#define _SRC_COMPILER_H_

#include <stdint.h> // uint32_t
#include <string.h> // memcpy

namespace dotlang {

typedef void (*CompiledFunction)(void);

class Guard {
 public:
  Guard(char* buffer, uint32_t length);
  ~Guard();

  inline CompiledFunction AsFunction() {
    return reinterpret_cast<CompiledFunction>(buffer_);
  }

 private:
  void* buffer_;
  void* guard_;
  uint32_t length_;
  uint32_t page_size_;
};

class CompiledScript {
 public:
  CompiledScript(const char* source, uint32_t length) {
    source_ = new char[length];
    length_ = length;
    memcpy(source_, source, length);

    Compile();
  }

  ~CompiledScript() {
    delete source_;
    delete guard_;
  }

  void Compile();
  void Run();

 private:
  Guard* guard_;

  char* source_;
  uint32_t length_;
};

} // namespace dotlang

#endif // _SRC_COMPILER_H_
