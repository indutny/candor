#include "compiler.h"
#include "parser.h"
#include "heap.h"
#include "fullgen.h"
#include "utils.h" // GetPageSize

#include <string.h> // memcpy, memset
#include <sys/mman.h> // mmap

namespace candor {

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
  heap_ = new Heap(2 * 1024 * 1024);

  {
    Zone zone;
    Parser p(source_, length_);
    Fullgen f(heap_);

    AstNode* ast = p.Execute();

    // Add scope information to variables (i.e. stack vs context, and indexes)
    Scope::Analyze(ast);

    // Generate machine code
    f.Generate(ast);

    // Allocate root context
    root_context_ = f.AllocateRoot();

    guard_ = new Guard(f.buffer(), f.length());
    f.Relocate(guard_->buffer());
  }
}


char* CompiledScript::Run() {
  // Context is undefined for main function
  // (It'll allocate new for itself)
  return guard_->AsFunction()(NULL, 0, root_context_);
}


bool CompiledScript::CaughtException() {
  return heap_->pending_exception() != NULL;
}


Guard::Guard(char* buffer, uint32_t length) {
  page_size_ = GetPageSize();

  length_ = RoundUp(length, page_size_);

  buffer_ = mmap(0,
                 length_,
                 PROT_READ | PROT_WRITE| PROT_EXEC,
                 MAP_ANON | MAP_PRIVATE,
                 -1,
                 0);
  if (buffer_ == MAP_FAILED) abort();

  memcpy(buffer_, buffer, length);
  memset(reinterpret_cast<char*>(buffer_) + length, 0xCC, length_ - length);

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

} // namespace candor
