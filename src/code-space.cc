#include "code-space.h"
#include "candor.h" // Error
#include "heap.h" // Heap
#include "heap-inl.h" // Heap
#include "parser.h" // Parser
#include "scope.h" // Scope
#include "macroassembler.h" // Masm
#include "hir.h" // HIR
#include "hir-inl.h" // HIR
#include "lir.h" // LIR
#include "lir-inl.h" // LIR
#include "stubs.h" // EntryStub
#include "utils.h" // GetPageSize

#include <sys/types.h> // off_t
#include <stdlib.h> // NULL
#include <string.h> // memcpy, memset
#include <sys/mman.h> // mmap

namespace candor {
namespace internal {

CodeSpace::CodeSpace(Heap* heap) : heap_(heap) {
  stubs_ = new Stubs(this);
  entry_ = stubs()->GetEntryStub();
}


CodeSpace::~CodeSpace() {
  delete stubs_;
}


Error* CodeSpace::CreateError(const char* filename,
                              const char* source,
                              uint32_t length,
                              const char* message,
                              uint32_t offset) {
  Error* err = new Error();

  err->message = message;
  err->line = GetSourceLineByOffset(source, offset, &err->offset);

  err->filename = filename;
  err->source = source;
  err->length = length;

  return err;
}


char* CodeSpace::Put(Masm* masm) {
  masm->AlignCode();

  char* code = Insert(masm->buffer(), masm->offset());
  masm->Relocate(code);

  return code;
}


char* CodeSpace::Compile(const char* filename,
                         const char* source,
                         uint32_t length,
                         char** root,
                         Error** error) {
  Zone zone;
  Parser p(source, length);

  AstNode* ast = p.Execute();

  // Set default filename, if no was given
  if (filename == NULL) {
    filename = "???";
  }

  if (p.has_error()) {
    *error = CreateError(filename,
                         source,
                         length,
                         p.error_msg(),
                         p.error_pos());
    return NULL;
  }

  // Add scope information to variables (i.e. stack vs context, and indexes)
  Scope::Analyze(ast);

  // Generate CFG with SSA
  HIRGen hir(heap(), ast);

  // Store root
  *root = hir.root()->Allocate()->addr();

  // Generate low-level representation
  Masm masm(this);

  // For each root in reverse order generate lir
  // (Generate children first, parents later)
  HIRBlockList::Item* head = hir.roots()->tail();
  for (; head != NULL; head = head->prev()) {
    // Generate LIR
    LGen lir(&hir, head->value());

    // Generate Masm code
    lir.Generate(&masm);
  }

  // Put code into code space
  char* addr = Put(&masm);

  // Relocate source map
  heap()->source_map()->Commit(filename, source, length, addr);

  return addr;
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
  if (fn == HNil::New() || HValue::IsUnboxed(fn)) {
    return reinterpret_cast<Value*>(HNil::New());
  }

  return reinterpret_cast<Code>(entry_)(fn,
                                        HNumber::Tag(argc),
                                        argv);
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
