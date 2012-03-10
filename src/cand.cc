#include "candor.h"

#include <stdio.h> // fprintf
#include <stdlib.h> // abort
#include <unistd.h> // open, lseek
#include <fcntl.h> // O_RDONLY, ...
#include <sys/types.h> // off_t
#include <string.h> // memcpy

#include <readline/readline.h>
#include <readline/history.h>

const char* ReadContents(const char* filename, off_t* size) {
  int fd = open(filename, O_RDONLY, S_IRUSR | S_IRGRP);
  if (fd == -1) {
    fprintf(stderr, "init: failed to open file %s\n", filename);
    abort();
  }

  off_t s = lseek(fd, 0, SEEK_END);
  if (s == -1) {
    fprintf(stderr, "init: failed to get filesize of %s\n", filename);
    abort();
  }

  char* contents = new char[s];
  if (pread(fd, contents, s, 0) != s) {
    fprintf(stderr, "init: failed to get contents of %s\n", filename);
    delete contents;
    abort();
  }

  close(fd);

  *size = s;
  return contents;
}


const char* StringToChar(candor::String* str) {
  char* result = new char[str->Length() + 1];

  memcpy(result, str->Value(), str->Length());
  result[str->Length()] = 0;

  return result;
}


candor::Value* APIAssert(uint32_t argc, candor::Arguments& argv) {
  if (argc < 1) {
    fprintf(stderr, "assert(): at least one argument is required\n");
    abort();
  }

  if (argv[0]->ToBoolean()->IsFalse()) {
    if (argc >= 2) {
      const char* text = StringToChar(argv[1]->ToString());
      fprintf(stderr, "assert(): assertion failed (%s)\n", text);
      delete text;
    } else {
      fprintf(stderr, "assert(): assertion failed\n");
    }
    abort();
  }

  return candor::Boolean::True();
}


candor::Value* APIPrint(uint32_t argc, candor::Arguments& argv) {
  if (argc < 1) return candor::Nil::New();

  const char* value = StringToChar(argv[0]->ToString());

  fprintf(stdout, "%s\n", value);

  delete value;

  return candor::Nil::New();
}


candor::Object* CreateGlobal() {
  candor::Object* obj = candor::Object::New();

  obj->Set("assert", candor::Function::New(APIAssert));
  obj->Set("print", candor::Function::New(APIPrint));

  return obj;
}


char* PrependGlobals(char* cmd) {
  const char* globals = "scope print, assert\n";
  char* result = new char[strlen(globals) + strlen(cmd) + 1];

  memcpy(result, globals, strlen(globals));
  memcpy(result + strlen(globals), cmd, strlen(cmd));

  // Readline is using malloc()
  free(cmd);

  return result;
}


void StartRepl() {
  candor::Object* global = CreateGlobal();

  while (true) {
    char* cmd = readline("> ");

    cmd = PrependGlobals(cmd);

    candor::Function* cmdfn = candor::Function::New(cmd, strlen(cmd));
    cmdfn->SetContext(global);

    candor::Value* args[0];
    candor::Value* result = cmdfn->Call(0, args);

    if (!result->Is<candor::Nil>()) {
      const char* value = StringToChar(result->ToString());
      fprintf(stdout, "%s\n", value);
      delete value;
    }

    delete cmd;
  }
}


int main(int argc, char** argv) {
  candor::Isolate isolate;

  if (argc < 2) {
    // Start repl
    StartRepl();
  } else {
    // Load script and run
    off_t size = 0;
    const char* script = ReadContents(argv[1], &size);

    candor::Function* code = candor::Function::New(script, size);
    delete script;

    if (isolate.HasSyntaxError()) {
      isolate.PrintSyntaxError();
      exit(1);
    }

    candor::Value* args[0];

    code->SetContext(CreateGlobal());

    return code->Call(0, args)->ToNumber()->IntegralValue();
  }
}
