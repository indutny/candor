#include "code-space.h"
#include "heap.h" // Heap
#include "heap-inl.h" // Heap
#include "parser.h" // Parser
#include "scope.h" // Scope
#include "fullgen.h" // Fullgen
#include "utils.h" // GetPageSize

#include <sys/types.h> // off_t
#include <stdlib.h> // NULL
#include <string.h> // memcpy, memset
#include <sys/mman.h> // mmap

namespace candor {
namespace internal {

CodeSpace::CodeSpace(Heap* heap) : heap_(heap) {
  pages_.allocated = true;
}


char* CodeSpace::Compile(const char* source, uint32_t length) {
  Zone zone;
  Parser p(source, length);
  Fullgen f(heap());

  AstNode* ast = p.Execute();

  // Add scope information to variables (i.e. stack vs context, and indexes)
  Scope::Analyze(ast);

  // Generate machine code
  uint32_t offset = f.Generate(ast);

  char* code = Insert(f.AllocateRoot(), offset, f.buffer(), f.length());
  f.Relocate(code);

  return code;
}


char* CodeSpace::Insert(char* root,
                        off_t offset,
                        char* code,
                        uint32_t length) {
  char* offsetptr = reinterpret_cast<char*>(offset);
  uint32_t total = length + sizeof(root) + sizeof(offsetptr);

  CodePage* page = NULL;

  // Go through pages to find enough space
  List<CodePage*, EmptyClass>::Item* item = pages_.head();
  while (item != NULL) {
    if (item->value()->Has(total)) {
      page = item->value();
      break;
    }
  }

  // If failed - allocate new page
  if (page == NULL) {
    page = new CodePage(total);
    pages_.Push(page);
  }

  char* space = page->Allocate(total);
  *reinterpret_cast<char**>(space) = offsetptr;
  *(reinterpret_cast<char**>(space) + 1) = root;

  char* cspace = space + 2 * sizeof(void*);
  memcpy(cspace, code, length);

  return cspace;
}


Value* CodeSpace::Run(char* fn,
                      Object* context,
                      uint32_t argc,
                      Value* argv[]) {
  char* root = *(reinterpret_cast<char**>(fn) - 1);
  off_t offset = *(reinterpret_cast<off_t*>(fn) - 2);

  // Set new context
  if (context != NULL) {
    // Note: that context have 0 index in root register
    HContext* hroot = HValue::As<HContext>(root);

    *reinterpret_cast<Object**>(hroot->GetSlotAddress(0)) = context;
  }

  return reinterpret_cast<Code>(fn)(root,
                                    HNumber::Tag(argc),
                                    argv,
                                    fn + offset);
}


CodePage::CodePage(uint32_t size) : offset_(0) {
  size_ = RoundUp(size, GetPageSize());

  page_ = reinterpret_cast<char*>(mmap(0,
                                       size_,
                                       PROT_READ | PROT_WRITE| PROT_EXEC,
                                       MAP_ANON | MAP_PRIVATE,
                                       -1,
                                       0));
  if (page_ == MAP_FAILED) abort();

  memset(page_, 0xCC, size_);

  guard_size_ = GetPageSize();
  guard_ = reinterpret_cast<char*>(mmap(page_ + size_,
                                        guard_size_,
                                        PROT_NONE,
                                        MAP_ANON | MAP_PRIVATE,
                                        -1,
                                        0));
  if (guard_ == MAP_FAILED) abort();
}


CodePage::~CodePage() {
  munmap(page_, size_);
  munmap(guard_, guard_size_);
}


bool CodePage::Has(uint32_t size) {
  return offset_ + size < size_;
}


char* CodePage::Allocate(uint32_t size) {
  offset_ += size;
  return page_ + offset_ - size;
}

} // namespace internal
} // namespace candor
