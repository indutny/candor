#include "compiler.h"
#include "parser.h"
#include "fullgen.h"

#include <string.h> // memcpy, memset
#include <unistd.h> // sysconf or getpagesize
#include <sys/mman.h> // mmap

namespace dotlang {

void CompiledScript::Compile() {
  Parser p(source_, length_);
  Fullgen f;

  AstNode* ast = p.Execute();

  f.GeneratePrologue();
  f.GenerateBlock(reinterpret_cast<BlockStmt*>(ast));
  f.GenerateEpilogue();

  guard_ = new Guard(f.buffer(), f.length());

  delete ast;
}


void CompiledScript::Run() {
  guard_->AsFunction()();
}


Guard::Guard(char* buffer, uint32_t length) {
#ifdef __DARWIN
  page_size_ = getpagesize();
#else
  page_size_ = sysconf(_SC_PAGE_SIZE);
#endif

  length_ = length;
  if (length_ % page_size_ != 0) {
    length_ += page_size_ - length_ % page_size_;
  }

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
