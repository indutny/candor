#include "candor.h"
#include "utils.h" // candor::internal::List

#include <stdio.h> // fprintf
#include <stdlib.h> // abort
#include <unistd.h> // open, lseek
#include <fcntl.h> // O_RDONLY, ...
#include <sys/types.h> // off_t
#include <string.h> // memcpy

typedef candor::internal::List<char*, candor::internal::EmptyClass> List;

const char* ReadContents(const char* filename, off_t* size) {
  int fd = open(filename, O_RDONLY, S_IRUSR | S_IRGRP);
  if (fd == -1) {
    fprintf(stderr, "init: failed to open file %s\n", filename);
    exit(1);
  }

  off_t s = lseek(fd, 0, SEEK_END);
  if (s == -1) {
    fprintf(stderr, "init: failed to get filesize of %s\n", filename);
    exit(1);
  }

  char* contents = new char[s];
  if (pread(fd, contents, s, 0) != s) {
    fprintf(stderr, "init: failed to get contents of %s\n", filename);
    delete contents;
    exit(1);
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


candor::Value* APIAssert(uint32_t argc, candor::Value* argv[]) {
  if (argc < 1) {
    fprintf(stderr, "assert(): at least one argument is required\n");
    abort();
  }

  if (argv[0]->ToBoolean()->IsFalse()) {
    if (argc >= 2) {
      const char* text = StringToChar(argv[1]->ToString());
      fprintf(stderr, "assert(): assertion failed (%s) in\n", text);
      delete text;
    } else {
      fprintf(stderr, "assert(): assertion failed in\n");
    }

    candor::Array* trace = candor::Isolate::GetCurrent()->StackTrace();
    assert(trace->Length() > 0);
    candor::Object* first = trace->Get(0)->As<candor::Object>();
    candor::String* filename = first->Get("filename")->As<candor::String>();
    int line = first->Get("line")->As<candor::Number>()->Value();

    fprintf(stderr,
            "          %.*s:%d\n",
            filename->Length(),
            filename->Value(),
            line);
    abort();
  }

  return candor::Boolean::True();
}


candor::Value* APIPrint(uint32_t argc, candor::Value* argv[]) {
  if (argc < 1) return candor::Nil::New();

  for (int i = 0; i < argc; i++) {
    const char* value = StringToChar(argv[i]->ToString());

    fprintf(stdout, i != (argc - 1) ? "%s " : "%s\n", value);

    delete value;
  }

  return candor::Nil::New();
}


candor::Value* APIToString(uint32_t argc, candor::Value* argv[]) {
  if (argc < 1) return candor::Nil::New();

  argv[0]->ToString();

  return candor::Nil::New();
}


candor::Object* CreateGlobal() {
  candor::Object* obj = candor::Object::New();

  obj->Set("assert", candor::Function::New(APIAssert));
  obj->Set("print", candor::Function::New(APIPrint));
  obj->Set("getValue", candor::Function::New(APIToString));

  return obj;
}


char* PrependGlobals(List* list) {
  int length = 0;

  // Get total length
  List::Item* item = list->head();
  while (item != NULL) {
    length += strlen(item->value()) + 1;
    item = item->next();
  }

  // Allocate string
  char* result = new char[length];

  // Copy all strings into it
  int offset = 0;

  item = list->head();
  while (item != NULL) {
    memcpy(result + offset, item->value(), strlen(item->value()) + 1);
    offset += strlen(item->value()) + 1;

    item = item->next();
    // Insert newlines between strings
    if (item != NULL) result[offset - 1] = '\n';
  }

  return result;
}


void StartRepl() {
  candor::Isolate isolate;
  candor::Object* global = CreateGlobal();

  List list;

  bool multiline = false;

  while (true) {
    char* cmd = new char[1000];
    fprintf(stdout, multiline ? "...   " : "can> ");
    fgets(cmd, 1000, stdin);

    // Replace '\n' with '\0'
    cmd[strlen(cmd) - 1] = 0;

    // Push item to the list
    list.Push(cmd);

    const char* prepended = PrependGlobals(&list);
    candor::Function* cmdfn = candor::Function::New("repl",
                                                    prepended,
                                                    strlen(prepended));

    if (!multiline || strlen(cmd) != 0) {

      // Continue collecting string on syntax error
      if (isolate.HasError()) {
        delete prepended;
        multiline = true;
        continue;
      }
    }

    // Remove all collected lines
    delete prepended;
    multiline = false;
    while (list.length() != 0) delete list.Shift();

    if (cmdfn == NULL) continue;

    // Call compiled function
    cmdfn->SetContext(global);

    candor::Value* args[0];
    candor::Value* result = cmdfn->Call(0, args);

    // Print result
    if (!result->Is<candor::Nil>()) {
      const char* value = StringToChar(result->ToString());
      fprintf(stdout, "%s\n", value);
      delete value;
    }
  }
}


int main(int argc, char** argv) {
  if (argc < 2) {
    // Start repl
    StartRepl();
  } else {
    candor::Isolate isolate;

    // Load script and run
    off_t size = 0;
    const char* script = ReadContents(argv[1], &size);

    candor::Function* code = candor::Function::New(argv[1], script, size);
    delete script;

    if (isolate.HasError()) {
      isolate.PrintError();
      exit(1);
    }

    code->SetContext(CreateGlobal());

    candor::Value* args[0];
    int ret = code->Call(0, args)->ToNumber()->IntegralValue();
    fflush(stdout);
    return ret;
  }
}
