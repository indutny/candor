#include "code-space.h"
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
  entry_ = GenerateEntry();
}


char* CodeSpace::GenerateEntry() {
  Zone zone;
  Masm masm(heap());

  EntryStub e(&masm);

  e.Generate();
  masm.AlignCode();

  char* code = Insert(masm.buffer(), masm.offset());
  masm.Relocate(code);

  return code;
}


char* CodeSpace::Compile(const char* source, uint32_t length, char** root) {
  Zone zone;
  Parser p(source, length);
  Fullgen f(heap());

  AstNode* ast = p.Execute();

  // Add scope information to variables (i.e. stack vs context, and indexes)
  Scope::Analyze(ast);

  // Generate machine code
  f.Generate(ast);
  f.AlignCode();

  *root = f.AllocateRoot();

  char* code = Insert(f.buffer(), f.offset());
  f.Relocate(code);

  return code;
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


Value* CodeSpace::Run(char* fn,
                      Object* context,
                      uint32_t argc,
                      Value* argv[]) {
  char* code = HFunction::Code(fn);
  char* parent = HFunction::Parent(fn);
  char* root = HFunction::Root(fn);

  // Set new context
  if (context != NULL) {
    // Note: that context have 0 index in root register
    HContext* hroot = HValue::As<HContext>(root);

    *reinterpret_cast<Object**>(hroot->GetSlotAddress(0)) = context;
  }

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
