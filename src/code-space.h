/**
 * Copyright (c) 2012, Fedor Indutny.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _SRC_CODE_SPACE_H_
#define _SRC_CODE_SPACE_H_

#include "utils.h"  // List

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
class CodeChunk;
class Code;
class PIC;

typedef List<CodePage*, EmptyClass> CodePageList;
typedef List<CodeChunk*, EmptyClass> CodeChunkList;

class CodeSpace {
 public:
  typedef Value* (*Code)(char*, uint32_t, Value* []);

  explicit CodeSpace(Heap* heap);
  ~CodeSpace();

  void CollectGarbage();

  Error* CreateError(CodeChunk* chunk, const char* message, uint32_t offset);

  CodeChunk* CreateChunk(const char* filename,
                         const char* source,
                         uint32_t length);
  char* CreatePIC();

  void Put(CodeChunk* chunk, Masm* masm);
  char* Compile(const char* filename,
                const char* source,
                uint32_t length,
                char** root,
                Error** error);

  Value* Run(char* fn, uint32_t argc, Value* argv[]);

  inline Heap* heap() { return heap_; }
  inline Stubs* stubs() { return stubs_; }

 private:
  Heap* heap_;
  Stubs* stubs_;
  char* entry_;
  CodePageList pages_;
  List<PIC*, EmptyClass> pics_;
  CodeChunkList chunks_;
};

class CodePage {
 public:
  explicit CodePage(uint32_t size);
  ~CodePage();

  bool Has(uint32_t size);
  char* Allocate(uint32_t size);

  void Ref();
  void Unref();

 private:
  uint32_t offset_;
  uint32_t size_;
  uint32_t guard_size_;
  int ref_;
  char* page_;
  char* guard_;

  friend class CodeSpace;
};

class CodeChunk {
 public:
  CodeChunk(const char* filename, const char* source, uint32_t length);
  ~CodeChunk();

  void Ref();
  void Unref();

  inline const char* filename() { return filename_; }
  inline const char* source() { return source_; }
  inline uint32_t source_len() { return source_len_; }
  inline char* addr() { return addr_; }

 private:
  char* filename_;
  char* source_;
  uint32_t source_len_;
  CodePage* page_;
  char* addr_;
  int ref_;

  friend class CodeSpace;
};
}  // internal
}  // candor

#endif  // _SRC_CODE_SPACE_H_
