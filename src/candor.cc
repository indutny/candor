#include "candor.h"
#include "compiler.h"

#include <stdint.h> // uint32_t
#include <stdlib.h> // NULL

namespace candor {

Script::Script() {
  script = NULL;
}

Script::~Script() {
  delete script;
}


void Script::Compile(const char* source, uint32_t length) {
  if (script != NULL) delete script;
  script = new CompiledScript(source, length);
}


char* Script::Run() {
  return script->Run();
}


bool Script::CaughtException() {
  return script->CaughtException();
}

} // namespace candor
