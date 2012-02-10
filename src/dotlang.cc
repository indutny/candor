#include "dotlang.h"
#include "compiler.h"

#include <stdint.h> // uint32_t
#include <stdlib.h> // NULL

namespace dotlang {

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


void Script::Run() {
  script->Run();
}

} // namespace dotlang
