#include "candor.h"

#include <stdio.h> // fprintf
#include <stdlib.h> // abort
#include <unistd.h> // open, lseek
#include <fcntl.h> // O_RDONLY, ...
#include <sys/types.h> // off_t
#include <string.h> // memcpy

using namespace candor;

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


const char* StringToChar(String* str) {
  char* result = new char[str->Length() + 1];

  memcpy(result, str->Value(), str->Length());
  result[str->Length()] = 0;

  return result;
}


Value* APIAssert(uint32_t argc, Arguments& argv) {
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

  return Boolean::True();
}


Value* APIPrint(uint32_t argc, Arguments& argv) {
  if (argc < 1) return Nil::New();

  const char* value = StringToChar(argv[0]->ToString());

  fprintf(stdout, "%s\n", value);

  delete value;

  return Nil::New();
}


Object* CreateGlobal() {
  Object* obj = Object::New();

  obj->Set("assert", Function::New(APIAssert));
  obj->Set("print", Function::New(APIPrint));

  return obj;
}


int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stdout, "Usage: cand [filename]\n");
    exit(0);
  }

  off_t size = 0;
  const char* script = ReadContents(argv[1], &size);

  Isolate isolate;
  Function* code = Function::New(script, size);

  Object* global = CreateGlobal();

  Value* args[0];
  code->Call(global, 0, args);

  return 0;
}
