#include "code-space.h"
#include "candor.h" // Error
#include "heap.h" // Heap
#include "heap-inl.h" // Heap
#include "parser.h" // Parser
#include "scope.h" // Scope
#include "fullgen.h" // Fullgen, Masm
#include "stubs.h" // EntryStub
#include "utils.h" // GetPageSize

#include <sys/types.h> // off_t
#include <stdlib.h> // NULL
#include <string.h> // memcpy, memset
#include <sys/mman.h> // mmap

namespace candor {
namespace internal {

CodeSpace::CodeSpace(Heap* heap) : heap_(heap) {
  pages_.allocated = true;
  stubs_ = new Stubs(this);
  entry_ = stubs()->GetEntryStub();
}


CodeSpace::~CodeSpace() {
  delete stubs_;
}


char* CodeSpace::Put(Masm* masm) {
  masm->AlignCode();

  char* code = Insert(masm->buffer(), masm->offset());
  masm->Relocate(code);

  return code;
}


char* CodeSpace::Compile(const char* source,
                         uint32_t length,
                         char** root,
                         Error** error) {
  Zone zone;
  Parser p(source, length);
  Fullgen f(this);

  AstNode* ast = p.Execute();

  if (p.has_error()) {
    Error* err = new Error();

    err->message = p.error_msg();
    err->line = GetSourceLineByOffset(source, p.error_pos(), &err->offset);

    err->source = source;
    err->length = length;

    *error = err;

    return NULL;
  }

  // Add scope information to variables (i.e. stack vs context, and indexes)
  Scope::Analyze(ast);

  // Generate machine code
  f.Generate(ast);

  if (f.has_error()) {
    Error* err = new Error();

    err->message = f.error_msg();
    err->line = GetSourceLineByOffset(source, 0, &err->offset);

    err->source = source;
    err->length = length;

    *error = err;
  }

  // Store root
  *root = f.AllocateRoot();

  // Get address of code
  return Put(&f);
}


char* CodeSpace::Insert(char* code, uint32_t length) {
  CodePage* page = NULL;

  // Go through pages to find enough space
  List<CodePage*, EmptyClass>::Item* item = pages_.head();
  while (item != NULL) {
    if (item->value()->Has(length)) {
      page = item->value();
      break;
    }
    item = item->next();
  }

  // If failed - allocate new page
  if (page == NULL) {
    page = new CodePage(length);
    pages_.Push(page);
  }

  char* space = page->Allocate(length);

  memcpy(space, code, length);

  return space;
}


Value* CodeSpace::Run(char* fn, uint32_t argc, Value* argv[]) {
  if (fn == NULL) return NULL;

  char* code = HFunction::Code(fn);
  char* parent = HFunction::Parent(fn);
  char* root = HFunction::Root(fn);

  return reinterpret_cast<Code>(entry_)(root,
                                        HNumber::Tag(argc),
                                        argv,
                                        code,
                                        parent);
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
