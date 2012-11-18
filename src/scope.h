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

#ifndef _SRC_SCOPE_H_
#define _SRC_SCOPE_H_

#include <assert.h>  // assert

#include "utils.h"  // HashMap
#include "zone.h"  // HashMap
#include "visitor.h"  // Visitor

namespace candor {
namespace internal {

// Forward declarations
class AstNode;
class AstValue;
class Scope;
class ScopeAnalyze;

// Each AstVariable gets it's slot
// After parse end indexes will be allocated
class ScopeSlot : public ZoneObject {
 public:
  typedef ZoneList<ScopeSlot*> UseList;

  enum Type {
    kStack,
    kContext,
    kImmediate
  };

  explicit ScopeSlot(Type type) : type_(type),
                                  value_(NULL),
                                  index_(-1),
                                  depth_(0),
                                  use_count_(0) {
  }

  ScopeSlot(Type type, int32_t depth) : type_(type),
                                        value_(NULL),
                                        index_(depth < 0 ? 0 : -1),
                                        depth_(depth),
                                        use_count_(0) {
  }

  static void Enumerate(void* scope, ScopeSlot* slot);

  inline bool is_stack() { return type_ == kStack; }
  inline bool is_context() { return type_ == kContext; }
  inline bool is_immediate() { return type_ == kImmediate; }

  inline void type(Type type) { type_ = type; }
  inline bool is_equal(ScopeSlot* to) {
    return type_ == to->type_ && index_ == to->index_ &&
           depth_ == to->depth_ && value_ == to->value_;
  }

  // Register slots should have a value (unboxed nil or number)
  inline char* value() {
    assert(is_immediate());
    return value_;
  }
  inline void value(char* value) {
    assert(is_immediate());
    value_ = value;
  }

  inline int32_t index() { return index_; }
  inline void index(int32_t index) { index_ = index; }
  inline int32_t depth() { return depth_; }
  inline void depth(int32_t depth) { depth_ = depth; }

  inline void use() { use_count_++; }
  inline int use_count() { return use_count_; }

  inline UseList* uses() { return &uses_; }

  void Print(PrintBuffer* p);

 protected:
  Type type_;
  char* value_;

  int32_t index_;
  int32_t depth_;
  int use_count_;

  UseList uses_;
};

// On each block or function enter new scope is created
class Scope : public ZoneMap<StringKey<ZoneObject>, ScopeSlot, ZoneObject> {
 public:
  enum Type {
    kBlock,
    kFunction
  };

  static void Analyze(AstNode* ast);

  Scope(ScopeAnalyze* a, Type type);
  ~Scope();

  ScopeSlot* GetSlot(const char* name, uint32_t length);

  inline int32_t stack_count() { return stack_count_; }
  inline int32_t context_count() { return context_count_; }
  inline Scope* parent() { return parent_; }
  inline Type type() { return type_; }

 protected:
  int32_t stack_count_;
  int32_t context_count_;

  int32_t stack_index_;
  int32_t context_index_;

  int32_t depth_;

  ScopeAnalyze* a_;
  Type type_;

  Scope* parent_;

  friend class ScopeSlot;
  friend class ScopeAnalyze;
};

// Runs on complete AST tree to wrap each kName into AstValue
// and put it either in stack or in some context
class ScopeAnalyze : public Visitor<AstNode> {
 public:
  explicit ScopeAnalyze(AstNode* ast);

  AstNode* VisitFunction(AstNode* node);
  AstNode* VisitCall(AstNode* node);
  AstNode* VisitName(AstNode* node);
  void VisitChildren(AstNode* node);

  inline Scope* scope() { return scope_; }

 protected:
  AstNode* ast_;
  Scope* scope_;

  friend class Scope;
};

}  // namespace internal
}  // namespace candor

#endif  // _SRC_SCOPE_H_
