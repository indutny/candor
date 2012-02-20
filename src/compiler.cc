#include "compiler.h"
#include "parser.h"
#include "heap.h"
#include "fullgen.h"

#include <string.h> // memcpy, memset
#include <unistd.h> // sysconf or getpagesize
#include <sys/mman.h> // mmap

namespace dotlang {

CompiledScript::CompiledScript(const char* source, uint32_t length) {
  // Copy source
  source_ = new char[length];
  length_ = length;
  memcpy(source_, source, length);

  // Compile
  Compile();
}


CompiledScript::~CompiledScript() {
  delete[] source_;
  delete guard_;
  delete heap_;
}


void CompiledScript::Compile() {
  // XXX: Hardcoded page size here
  heap_ = new Heap(1024 * 1024);

  Zone zone;
  Parser p(source_, length_);
  Fullgen f(heap_);

  AstNode* ast = p.Execute();

  // Add scope information to variables (i.e. stack vs context, and indexes)
  Scope::Analyze(ast);

  // Generate machine code
  f.Visit(ast);

  guard_ = new Guard(f.buffer(), f.length());
}


void CompiledScript::Run() {
  guard_->AsFunction()(heap_);
}


Guard::Guard(char* buffer, uint32_t length) {
  // TODO: move page size getting into utils.h
#ifdef __DARWIN
  page_size_ = getpagesize();
#else
  page_size_ = sysconf(_SC_PAGE_SIZE);
#endif

  length_ = RoundUp(length, page_size_);

  buffer_ = mmap(0,
                 length_,
                 PROT_READ | PROT_WRITE| PROT_EXEC,
                 MAP_ANON | MAP_PRIVATE,
                 -1,
                 0);
  if (buffer_ == MAP_FAILED) abort();

  memcpy(buffer_, buffer, length);
  memset(reinterpret_cast<char*>(buffer_) + length, 0x90, length_ - length);

  guard_ = mmap(reinterpret_cast<char*>(buffer_) + length_,
                page_size_,
                PROT_NONE,
                MAP_ANON | MAP_PRIVATE,
                -1,
                0);
  if (guard_ == MAP_FAILED) abort();
}


Guard::~Guard() {
  munmap(buffer_, length_);
  munmap(guard_, page_size_);
}

} // namespace dotlang
