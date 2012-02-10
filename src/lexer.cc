#include "lexer.h"
#include <string.h> // strncmp
#include <stdlib.h> // NULL

#define LENGTH_CHECK\
    if (offset == length) return new Token(kEnd, offset);

#define CHECK_KEYWORD(value, length, exp_length, exp_value, type)\
    if (length == exp_length && strncmp(value, exp_value, exp_length) == 0) {\
      return new Token(type, value, length, start);\
    }

#define MATH_ONE(c1, type)\
    if (get(0) == c1) {\
      offset++;\
      return new Token(type, offset - 1);\
    }

#define MATH_TWO(c1, c2, type)\
    if (get(0) == c1 && get(1) == c2) {\
      offset += 2;\
      return new Token(type, offset - 2);\
    }

#define MATH_THREE(c1, c2, c3, type)\
    if (get(0) == c1 && get(1) == c2 && get(2) == c3) {\
      offset += 3;\
      return new Token(type, offset - 3);\
    }

namespace dotlang {

Lexer::Token* Lexer::Peek() {
  List<Token*>::Item* head = queue.Head();
  if (head != NULL) return head->value;

  Token* result = Consume();
  queue.Push(result);

  return result;
}


void Lexer::Skip() {
  delete queue.Shift();
}


Lexer::Token* Lexer::Consume() {
  // Skip spaces and detect CR
  bool cr = false;
  while (offset < length &&
         (get(0) == ' ' || get(0) == '\r' || get(0) == '\n')) {
    if (get(0) == '\r' || get(0) == '\n') cr = true;
    offset++;
  }

  if (cr) return new Token(kCr, offset - 1);

  LENGTH_CHECK

  // One-line comment
  if (offset + 1 < length && get(0) == '/' && get(1) == '/') {
    offset += 2;
    uint32_t start = offset;
    while (offset < length && get(0) != '\r' && get(0) != '\n') offset++;

    return new Token(kComment, source + start, offset - start, start);
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
      offset++;
      return new Token(type, offset - 1);
    }
  }

  // Number
  if (get(0) >= '0' && get(0) <= '9') {
    uint32_t start = offset;
    while (offset < length && get(0) >= '0' && get(0) <= '9') offset++;
    return new Token(kNumber, source + start, offset - start, start);
  }

  // String
  if (get(0) == '"' || get(0) == '\'') {
    offset++;

    uint32_t start = offset;
    char endchar = get(0);

    while (offset < length) {
      if (get(0) == endchar) break;
      if (get(0) == '\\' ) {
        // Skip escaped char
        offset++;
        LENGTH_CHECK
      }
      offset++;
    }

    if (offset == length && get(0) != endchar) return new Token(kEnd, offset);

    // Note: token value will contain \c escaped chars
    return new Token(kString, source + start, offset - 1 - start, start);
  }

  // Math
  {
    // Three char ops
    if (offset + 2 < length) {
      MATH_THREE('=', '=', '=', kStrictEq)
      MATH_THREE('!', '=', '=', kStrictNe)
    }

    // Two char ops
    if (offset + 1 < length) {
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
    uint32_t start = offset;
    while (offset < length &&
           (get(0) >= 'a' && get(0) <= 'z' ||
           get(0) >= 'A' && get(0) <= 'Z' ||
           get(0) >= '0' && get(0) <= '9')) {
      offset++;
    }

    const char* value = source + start;
    uint32_t len = offset - start;

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

    if (len == 0) return new Token(kEnd, offset);

    return new Token(kName, value, len, start);
  }
}

}
