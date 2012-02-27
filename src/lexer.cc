#include "lexer.h"
#include <string.h> // strncmp
#include <stdlib.h> // NULL
#include "zone.h" // ZoneObject

#define LENGTH_CHECK\
    if (offset_ == length_) return new Token(kEnd, offset_);

#define CHECK_KEYWORD(value, length, exp_length, exp_value, type)\
    if (length == exp_length && strncmp(value, exp_value, exp_length) == 0) {\
      return new Token(type, value, length, start);\
    }

#define MATH_ONE(c1, type)\
    if (get(0) == c1) {\
      offset_++;\
      return new Token(type, offset_ - 1);\
    }

#define MATH_TWO(c1, c2, type)\
    if (get(0) == c1 && get(1) == c2) {\
      offset_ += 2;\
      return new Token(type, offset_ - 2);\
    }

#define MATH_THREE(c1, c2, c3, type)\
    if (get(0) == c1 && get(1) == c2 && get(2) == c3) {\
      offset_ += 3;\
      return new Token(type, offset_ - 3);\
    }

namespace dotlang {

Lexer::Token* Lexer::Peek() {
  List<Token*, ZoneObject>::Item* head = queue()->head();
  if (head != NULL) return head->value();

  Token* result = Consume();
  queue()->Push(result);

  return result;
}


void Lexer::Skip() {
  queue()->Shift();
}


Lexer::Token* Lexer::Consume() {
  // Skip spaces and detect CR
  bool cr = false;
  while (has(1) && (get(0) == ' ' || get(0) == '\r' || get(0) == '\n')) {
    if (get(0) == '\r' || get(0) == '\n') cr = true;
    offset_++;
  }

  if (cr) return new Token(kCr, offset_ - 1);

  LENGTH_CHECK

  // One-line comment
  if (has(2) && get(0) == '/' && get(1) == '/') {
    offset_ += 2;
    uint32_t start = offset_;
    while (offset_ < length_ && get(0) != '\r' && get(0) != '\n') offset_++;

    return new Token(kComment, source_ + start, offset_ - start, start);
  }

  // One-char tokens
  {
    TokenType type = kEnd;
    switch (get(0)) {
     case '.':
      type = kDot;
      break;
     case ',':
      type = kComma;
      break;
     case ':':
      type = kColon;
      break;
     case '(':
      type = kParenOpen;
      break;
     case ')':
      type = kParenClose;
      break;
     case '{':
      type = kBraceOpen;
      break;
     case '}':
      type = kBraceClose;
      break;
     case '[':
      type = kArrayOpen;
      break;
     case ']':
      type = kArrayClose;
      break;
     default:
      break;
    }

    if (type != kEnd) {
      offset_++;
      return new Token(type, offset_ - 1);
    }
  }

  // Number
  if (get(0) >= '0' && get(0) <= '9') {
    uint32_t start = offset_;
    do { offset_++; } while (has(1) && get(0) >= '0' && get(0) <= '9');
    return new Token(kNumber, source_ + start, offset_ - start, start);
  }

  // String
  if (get(0) == '"' || get(0) == '\'') {
    char endchar = get(0);
    uint32_t start = ++offset_;

    while (has(1)) {
      if (get(0) == endchar) break;
      if (get(0) == '\\' ) {
        // Skip escaped char
        offset_++;
        LENGTH_CHECK
      }
      offset_++;
    }

    if (!has(1) || get(0) != endchar) return new Token(kEnd, offset_);
    // Skip endchar
    offset_++;

    // Note: token value will contain \c escaped chars
    return new Token(kString, source_ + start, offset_ - 1 - start, start);
  }

  // Math
  {
    // Three char ops
    if (has(3)) {
      MATH_THREE('=', '=', '=', kStrictEq)
      MATH_THREE('!', '=', '=', kStrictNe)
    }

    // Two char ops
    if (has(2)) {
      MATH_TWO('+', '+', kInc)
      MATH_TWO('-', '-', kDec)
      MATH_TWO('=', '=', kEq)
      MATH_TWO('>', '=', kGe)
      MATH_TWO('<', '=', kLe)
      MATH_TWO('!', '=', kNe)
      MATH_TWO('|', '|', kLOr)
      MATH_TWO('&', '&', kLAnd)
    }

    MATH_ONE('+', kAdd)
    MATH_ONE('-', kSub)
    MATH_ONE('/', kDiv)
    MATH_ONE('*', kMul)
    MATH_ONE('<', kLt)
    MATH_ONE('>', kGt)
    MATH_ONE('!', kNot)
    MATH_ONE('&', kBAnd)
    MATH_ONE('|', kBOr)
    MATH_ONE('^', kBXor)
    MATH_ONE('=', kAssign)
  }

  // Name or keyword
  {
    uint32_t start = offset_;
    while (has(1) &&
           (get(0) >= 'a' && get(0) <= 'z' ||
           get(0) >= 'A' && get(0) <= 'Z' ||
           get(0) >= '0' && get(0) <= '9')) {
      offset_++;
    }

    const char* value = source_ + start;
    uint32_t len = offset_ - start;

    CHECK_KEYWORD(value, len, 2, "if", kIf)
    CHECK_KEYWORD(value, len, 3, "new", kNew)
    CHECK_KEYWORD(value, len, 3, "nil", kNil)
    CHECK_KEYWORD(value, len, 4, "else", kElse)
    CHECK_KEYWORD(value, len, 4, "true", kTrue)
    CHECK_KEYWORD(value, len, 5, "scope", kScope)
    CHECK_KEYWORD(value, len, 5, "while", kWhile)
    CHECK_KEYWORD(value, len, 5, "break", kBreak)
    CHECK_KEYWORD(value, len, 5, "false", kFalse)
    CHECK_KEYWORD(value, len, 6, "return", kReturn)

    if (len == 0) return new Token(kEnd, offset_);

    return new Token(kName, value, len, start);
  }
}

} // namespace dotlang
