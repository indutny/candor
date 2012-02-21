#ifndef _INCLUDE_DOTLANG_H_
#define _INCLUDE_DOTLANG_H_

#include <stdint.h> // uint32_t

namespace dotlang {

class CompiledScript;

class Script {
 public:
  Script();
  ~Script();

  void Compile(const char* source, uint32_t length);
  void* Run();

 private:
  CompiledScript* script;
};

} // namespace dotlang

#endif // _INCLUDE_DOTLANG_H_
