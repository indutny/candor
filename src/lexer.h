#ifndef _SRC_LEXER_H_
#define _SRC_LEXER_H_

#include "utils.h" // List
#include <stdint.h>

namespace dotlang {

class Lexer {
 public:
  Lexer(const char* source, uint32_t length) : source_(source),
                                               offset_(0),
                                               length_(length) {
    queue_.allocated = true;
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

    inline bool is(TokenType type) {
      return type_ == type;
    }

    inline const uint32_t offset() {
      return offset_;
    }

    TokenType type_;
    const char* value_;
    uint32_t length_;
    uint32_t offset_;
  };

  Token* Peek();
  void Skip();
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

  inline List<Token*>* queue() {
    return &queue_;
  }

  const char* source_;
  uint32_t offset_;
  uint32_t length_;

  List<Token*> queue_;
};

} // namespace dotlang

#endif // _SRC_LEXER_H_
