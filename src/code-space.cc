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

#include "code-space.h"

#include <stdlib.h>  // NULL
#include <string.h>  // memcpy, memset
#include <sys/mman.h>  // mmap

#include "candor.h"  // Error
#include "heap.h"  // Heap
#include "heap-inl.h"  // Heap
#include "parser.h"  // Parser
#include "scope.h"  // Scope
#include "macroassembler.h"  // Masm
#include "root.h"  // Root
#include "fullgen.h"  // Fullgen
#include "fullgen-inl.h"  // Fullgen
#include "hir.h"  // HIR
#include "hir-inl.h"  // HIR
#include "lir.h"  // LIR
#include "lir-inl.h"  // LIR
#include "source-map.h"  // SourceMap
#include "stubs.h"  // EntryStub
#include "pic.h"  // PIC
#include "utils.h"  // GetPageSize

namespace candor {
namespace internal {

CodeSpace::CodeSpace(Heap* heap) : heap_(heap) {
  stubs_ = new Stubs(this);
  entry_ = stubs()->GetEntryStub();
  heap->code_space(this);
}


CodeSpace::~CodeSpace() {
  delete stubs_;
}


void CodeSpace::CollectGarbage() {
  // Remove unused chunks
  CodeChunkList::Item* chead = chunks_.head();
  CodeChunkList::Item* cnext;
  for (; chead != NULL; chead = cnext) {
    CodeChunk* chunk = chead->value();
    cnext = chead->next();

    if (chunk->ref_ == 0) chunks_.Remove(chead);
  }

  // Remove unused pages
  CodePageList::Item* phead = pages_.head();
  CodePageList::Item* pnext;
  for (; phead != NULL; phead = pnext) {
    CodePage* page = phead->value();
    pnext = phead->next();

    if (page->ref_ == 0) pages_.Remove(phead);
  }
}


Error* CodeSpace::CreateError(CodeChunk* chunk,
                              const char* message,
                              uint32_t offset) {
  Error* err = new Error();

  err->message = message;
  err->line = GetSourceLineByOffset(chunk->source(), offset, &err->offset);

  err->filename = chunk->filename();
  err->source = chunk->source();
  err->length = chunk->source_len();

  return err;
}


CodeChunk* CodeSpace::CreateChunk(const char* filename,
                                  const char* source,
                                  uint32_t length) {
  // Remove unused code data first
  CollectGarbage();

  CodeChunk* c = new CodeChunk(filename, source, length);
  chunks_.Push(c);

  return c;
}


void CodeSpace::Put(CodeChunk* chunk, Masm* masm) {
  // Align code in chunk
  masm->AlignCode();

  char* code = masm->buffer();
  uint32_t length = masm->offset();

  // Go through pages to find one with enough space
  CodePage* p = NULL;
  List<CodePage*, EmptyClass>::Item* item = pages_.head();
  while (item != NULL) {
    if (item->value()->Has(length)) {
      p = item->value();
      break;
    }
    item = item->next();
  }

  // If failed - allocate new page
  if (p == NULL) {
    p = new CodePage(length);
    pages_.Push(p);
  }

  // Copy code into executable memory
  chunk->page_ = p;
  chunk->addr_ = p->Allocate(length);
  memcpy(chunk->addr_, code, length);

  // Chunk now references page
  p->Ref();

  // Relocate references
  masm->Relocate(heap(), chunk->addr_);
}


char* CodeSpace::Compile(const char* filename,
                         const char* source,
                         uint32_t length,
                         char** root,
                         Error** error) {
  Zone zone;

  CodeChunk* chunk = CreateChunk(filename, source, length);

  Parser p(chunk->source(), chunk->source_len());

  AstNode* ast = p.Execute();

  if (p.has_error()) {
    *error = CreateError(chunk, p.error_msg(), p.error_pos());
    return NULL;
  }

  // Add scope chunkrmation to variables (i.e. stack vs context, and indexes)
  Scope::Analyze(ast);

  Root r(heap());
  Masm masm(this);

  for (FunctionIterator it(ast); !it.IsEnded(); it.Advance()) {
    FunctionLiteral* current = it.Value();

    int len = current->own_length();
    if (len < HIRGen::kMaxOptimizableSize) {
      // Generate CFG with SSA
      HIRGen hir(heap(), &r, chunk->filename());

      hir.Build(current);

      // Generate low-level representation:
      //   For each root in reverse order generate lir
      //   (Generate children first, parents later)
      HIRBlockList::Item* head = hir.roots()->head();
      for (; head != NULL; head = head->next()) {
        // Generate LIR
        LGen lir(&hir, chunk->filename(), head->value());

        // Generate Masm code
        lir.Generate(&masm, heap()->source_map());
      }
    } else {
      Fullgen f(heap(), &r, chunk->filename());

      // Create instruction list
      f.Build(current);

      // Generate instructions
      f.Generate(&masm);
    }
  }

  // Store root
  *root = r.Allocate()->addr();

  // Put code into code space
  Put(chunk, &masm);

  // Relocate source map
  heap()->source_map()->Commit(chunk->filename(),
                               chunk->source(),
                               chunk->source_len(),
                               chunk->addr());

  return chunk->addr();
}


char* CodeSpace::CreatePIC() {
  PIC* p = new PIC(this);

  pics_.Push(p);

  return p->Generate();
}


Value* CodeSpace::Run(char* fn, uint32_t argc, Value* argv[]) {
  if (fn == HNil::New() || HValue::IsUnboxed(fn)) {
    return reinterpret_cast<Value*>(HNil::New());
  }

  return reinterpret_cast<Code>(entry_)(fn,
                                        HNumber::Tag(argc),
                                        argv);
}


CodePage::CodePage(uint32_t size) : offset_(0), ref_(0) {
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


void CodePage::Ref() {
  ref_++;
}


void CodePage::Unref() {
  ref_--;
  assert(ref_ >= 0);
}


CodeChunk::CodeChunk(const char* filename, const char* source, uint32_t length)
    : source_len_(length), page_(NULL), ref_(1) {
  int filename_len = strlen(filename) + 1;

  filename_ = new char[filename_len];
  source_ = new char[source_len_];

  memcpy(filename_, filename, filename_len);
  memcpy(source_, source, source_len_);
}


CodeChunk::~CodeChunk() {
  delete[] filename_;
  delete[] source_;
  page_->Unref();
}


void CodeChunk::Ref() {
  ref_++;
}


void CodeChunk::Unref() {
  ref_--;
  assert(ref_ >= 0);
}

}  // namespace internal
}  // namespace candor
