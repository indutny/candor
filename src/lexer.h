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

#ifndef _SRC_LEXER_H_
#define _SRC_LEXER_H_

#include <stdint.h>

#include "utils.h"  // List
#include "zone.h"  // ZoneObject

namespace candor {
namespace internal {

#define LEX_TOKEN_TYPES(V) \
    V(Cr) \
    V(Dot) \
    V(Ellipsis) \
    V(Comma) \
    V(Colon) \
    V(Assign) \
    V(Comment) \
    V(ArrayOpen) \
    V(ArrayClose) \
    V(ParenOpen) \
    V(ParenClose) \
    V(BraceOpen) \
    V(BraceClose) \
    V(Inc) \
    V(Dec) \
    V(Add) \
    V(Sub) \
    V(Div) \
    V(Mul) \
    V(Mod) \
    V(BAnd) \
    V(BOr) \
    V(BXor) \
    V(Shl) \
    V(Shr) \
    V(UShr) \
    V(Eq) \
    V(StrictEq) \
    V(Ne) \
    V(StrictNe) \
    V(Lt) \
    V(Gt) \
    V(Le) \
    V(Ge) \
    V(LOr) \
    V(LAnd) \
    V(Not) \
    V(Number) \
    V(String) \
    V(False) \
    V(True) \
    V(Nan) \
    V(Nil) \
    V(Name) \
    V(If) \
    V(Else) \
    V(While) \
    V(Break) \
    V(Continue) \
    V(Return) \
    V(Clone) \
    V(Delete) \
    V(Typeof) \
    V(Sizeof) \
    V(Keysof) \
    V(End)

// Splits source code into lexems and emits them
class Lexer {
 public:
  Lexer(const char* source, uint32_t length) : source_(source),
                                               offset_(0),
                                               length_(length) {
  }

#define LEX_TOKEN_ENUM(V) \
    k##V,

  enum TokenType {
    LEX_TOKEN_TYPES(LEX_TOKEN_ENUM)
    kNone
  };

#undef LEX_TOKEN_ENUM

  class Token : public ZoneObject {
   public:
    Token(TokenType type, uint32_t offset) : type_(type),
                                             value_(NULL),
                                             length_(0),
                                             offset_(offset) {
    }

    Token(TokenType type,
          const char* value,
          uint32_t length,
          uint32_t offset) :
        type_(type), value_(value), length_(length), offset_(offset) {
    }

    inline TokenType type() {
      return type_;
    }

    inline bool is(TokenType type) { return type_ == type; }

#define LEX_TOKEN_CASE(V) \
    case k##V: r = #V; break;\

    inline const char* ToString() {
      const char* r = NULL;
      switch (type_) {
       LEX_TOKEN_TYPES(LEX_TOKEN_CASE)
       default: UNEXPECTED
      }

      return r;
    }

#undef LEX_TOKEN_CASE

    inline const char* value() { return value_; }
    inline uint32_t length() { return length_; }
    inline uint32_t offset() { return offset_; }

    TokenType type_;
    const char* value_;
    uint32_t length_;
    uint32_t offset_;
  };

  Token* Peek();
  void Skip();
  void SkipCr();
  bool SkipWhitespace();
  Token* Consume();

  void Save();
  void Restore();
  void Commit();

  inline char get(uint32_t delta) {
    return source_[offset_ + delta];
  }

  inline bool has(uint32_t num) {
    return offset_ + num - 1 < length_;
  }

  inline ZoneList<Token*>* queue() {
    return &queue_;
  }

  const char* source_;
  uint32_t offset_;
  uint32_t length_;

  ZoneList<Token*> queue_;
};

}  // namespace internal
}  // namespace candor

#endif  // _SRC_LEXER_H_
