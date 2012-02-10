#ifndef _SRC_LEXER_H_
#define _SRC_LEXER_H_

#include "utils.h" // List
#include <stdint.h>

namespace dotlang {

class Lexer {
 public:
  Lexer(const char* source_, uint32_t length_) : source(source_),
                                                 offset(0),
                                                 length(length_) {
    queue.allocated = true;
  }

  enum TokenType {
    // Punctuation
    kCr,
    kDot,
    kComma,
    kColon,
    kAssign,
    kComment,
    kArrayOpen,
    kArrayClose,
    kParenOpen,
    kParenClose,
    kBraceOpen,
    kBraceClose,

    // Math
    kInc,
    kDec,
    kAdd,
    kSub,
    kDiv,
    kMul,
    kBAnd,
    kBOr,
    kBXor,

    // Logic
    kEq,
    kStrictEq,
    kNe,
    kStrictNe,
    kLt,
    kGt,
    kLe,
    kGe,
    kLOr,
    kLAnd,
    kNot,

    // Literals
    kNumber,
    kString,
    kFalse,
    kTrue,
    kNil,

    // Various
    kName,

    // Keywords
    kIf,
    kElse,
    kWhile,
    kBreak,
    kReturn,
    kScope,
    kNew,
    kEnd
  };

  class Token {
   public:
    Token(TokenType type_, uint32_t offset_) : type(type_),
                                               value(NULL),
                                               length(0),
                                               offset(offset_) {
    }

    Token(TokenType type_,
          const char* value_,
          uint32_t length_,
          uint32_t offset_) :
        type(type_), value(value_), length(length_), offset(offset_) {
    }

    TokenType type;
    const char* value;
    uint32_t length;
    uint32_t offset;
  };

  Token* Peek();
  void Skip();
  Token* Consume();

  void Save();
  void Restore();
  void Commit();

  inline char get(uint32_t offset_) {
    return source[offset + offset_];
  }

  const char* source;
  uint32_t offset;
  uint32_t length;

  List<Token*> queue;
};

} // namespace dotlang

#endif // _SRC_LEXER_H_
