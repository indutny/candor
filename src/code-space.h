#ifndef _SRC_CODE_SPACE_H_
#define _SRC_CODE_SPACE_H_

#include "utils.h" // List

namespace candor {

// Forward declaration
class Value;
struct Error;

namespace internal {

// Forward declaration
class Heap;
class Masm;
class Stubs;
class CodePage;
class CodeInfo;
class PIC;

class CodeSpace {
 public:
  typedef Value* (*Code)(char*, uint32_t, Value* []);

  CodeSpace(Heap* heap);
  ~CodeSpace();

  Error* CreateError(const char* filename,
                     const char* source,
                     uint32_t length,
                     const char* message,
                     uint32_t offset);

  char* Put(Masm* masm);
  char* Compile(const char* filename,
                const char* source,
                uint32_t length,
                char** root,
                Error** error);
  char* Insert(char* code, uint32_t length);

  Value* Run(char* fn, uint32_t argc, Value* argv[]);

  inline Heap* heap() { return heap_; }
  inline Stubs* stubs() { return stubs_; }

 private:
  Heap* heap_;
  Stubs* stubs_;
  char* entry_;
  List<CodePage*, EmptyClass> pages_;
  List<PIC*, EmptyClass> pics_;
  List<CodeInfo*, EmptyClass> infos_;
};

class CodeInfo {
 public:
  CodeInfo(const char* filename, const char* source, uint32_t length);
  ~CodeInfo();

  inline const char* filename() { return filename_; }
  inline const char* source() { return source_; }
  inline uint32_t source_len() { return source_len_; }

 private:
  char* filename_;
  char* source_;
  uint32_t source_len_;
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
