#ifndef _INCLUDE_DOTLANG_H_
#define _INCLUDE_DOTLANG_H_

#include <stdint.h> // uint32_t

namespace candor {

class CompiledScript;

class Script {
 public:
  Script();
  ~Script();

  void Compile(const char* source, uint32_t length);
  char* Run();

  bool CaughtException();

 private:
  CompiledScript* script;
};

} // namespace candor

#endif // _INCLUDE_DOTLANG_H_
