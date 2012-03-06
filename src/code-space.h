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
  typedef Value* (*Code)(char*, uint32_t, Value* [], char*);

  CodeSpace(Heap* heap);

  char* Compile(const char* source, uint32_t length);
  char* Insert(char* root, off_t offset, char* code, uint32_t length);

  static Value* Run(char* fn, Object* context, uint32_t argc, Value* argv[]);

  inline Heap* heap() { return heap_; }

 private:
  Heap* heap_;
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
