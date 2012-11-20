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

#ifndef _SRC_PARSER_H_
#define _SRC_PARSER_H_

#include <assert.h>  // assert
#include <stdlib.h>  // NULL

#include "lexer.h"
#include "ast.h"
#include "utils.h"
#include "zone.h"

namespace candor {
namespace internal {

// Creates AST tree from plain source code
class Parser : public Lexer, public ErrorHandler {
 public:
  enum ParserSign {
    kNormal,
    kNegated
  };

  enum ParseStatementType {
    kSkipTrailingCr,
    kLeaveTrailingCr
  };

  enum PrimaryRestriction {
    kNoKeywords,
    kAny
  };

  Parser(const char* source, uint32_t length) : Lexer(source, length),
                                                ast_id_(0) {
    ast_ = Add(new FunctionLiteral(NULL));
    ast_->make_root();
    sign_ = kNormal;
  }

  // Used to implement lookahead
  class Position {
   public:
    explicit Position(Lexer* lexer) : lexer_(lexer), committed_(false) {
      if (lexer_->queue()->length() == 0) {
        offset_ = lexer_->offset_;
      } else {
        offset_ = lexer_->queue()->head()->value()->offset_;
      }
    }

    ~Position() {
      if (!committed_) {
        while (lexer_->queue()->length() > 0) lexer_->queue()->Shift();
        lexer_->offset_ = offset_;
      }
    }

    inline AstNode* Commit(AstNode* result) {
      if (result != NULL) {
        committed_ = true;
      }
      return result;
    }

   private:
    Lexer* lexer_;
    uint32_t offset_;
    bool committed_;
  };

  class NegateSign {
   public:
    NegateSign(Parser* p, TokenType type) : p_(p), sign_(p->sign_) {
      if (sign_ == kNormal && type == kSub) {
        p_->sign_ = kNegated;
      } else if (sign_ == kNegated && type == kAdd) {
        p_->sign_ = kNormal;
      }
    }

    ~NegateSign() {
      p_->sign_ = sign_;
    }
   private:
    Parser* p_;
    ParserSign sign_;
  };

  inline TokenType NegateType(TokenType type) {
    if (sign_ == kNormal) return type;

    switch (type) {
     case kAdd:
      return kSub;
     case kSub:
      return kAdd;
     default:
      return type;
    }
  }

  inline AstNode* Add(AstNode* node) {
    node->id = ast_id_++;
    return node;
  }

  // AST (result)
  inline AstNode* ast() {
    return ast_;
  }

  inline void SetError(const char* msg) {
    ErrorHandler::SetError(msg, Peek()->offset());
  }

  // Prints AST into buffer (debug purposes only)
  void Print(char* buffer, uint32_t size);

  AstNode* Execute();
  AstNode* ParseStatement(ParseStatementType type);
  AstNode* ParseExpression(int priority = 0);
  AstNode* ParsePrefixUnOp(TokenType type);
  AstNode* ParseBinOp(TokenType type, AstNode* lhs, int priority = 0);
  AstNode* ParsePrimary(PrimaryRestriction rest);
  AstNode* ParseMember();
  AstNode* ParseObjectLiteral();
  AstNode* ParseArrayLiteral();
  AstNode* ParseBlock(AstNode* block);

 protected:
  ParserSign sign_;
  ZoneList<FunctionLiteral*> fns_;

  AstNode* ast_;
  int ast_id_;
};

}  // namespace internal
}  // namespace candor

#endif  // _SRC_PARSER_H_
