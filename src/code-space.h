#ifndef _SRC_CODE_SPACE_H_
#define _SRC_CODE_SPACE_H_

#include "utils.h" // List

#include <sys/types.h> // off_t

namespace candor {

// Forward declaration
class Object;
class Value;

namespace internal {

// Forward declaration
class Heap;
class CodePage;

class CodeSpace {
 public:
  typedef Value* (*Code)(char*, uint32_t, Value* [], char*, char*);

  CodeSpace(Heap* heap);

  char* GenerateEntry();
  char* Compile(const char* source, uint32_t length, char** root);
  char* Insert(char* code, uint32_t length);

  Value* Run(char* fn, Object* context, uint32_t argc, Value* argv[]);

  inline Heap* heap() { return heap_; }

 private:
  Heap* heap_;
  char* entry_;
  List<CodePage*, EmptyClass> pages_;
};

class CodePage {
 public:
  CodePage(uint32_t size);
  ~CodePage();

  bool Has(uint32_t size);
  char* Allocate(uint32_t size);

 private:
  uint32_t offset_;
  uint32_t size_;
  uint32_t guard_size_;
  char* page_;
  char* guard_;
};

} // internal
} // candor

#endif // _SRC_CODE_SPACE_H_
