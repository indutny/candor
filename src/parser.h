#ifndef _SRC_PARSER_H_
#define _SRC_PARSER_H_

#include "lexer.h"
#include "ast.h"

#include <assert.h> // assert
#include <stdlib.h> // NULL

namespace dotlang {

class Parser : public Lexer {
 public:
  Parser(const char* source, uint32_t length) : Lexer(source, length) {
    ast = new BlockStmt(NULL);
    current = ast;
  }


  class Position {
   public:
    Position(Lexer* lexer_) : lexer(lexer_), committed(false) {
      if (lexer->queue.length == 0) {
        offset = lexer->offset;
      } else {
        offset = lexer->queue.Head()->value->offset;
      }
    }

    ~Position() {
      if (!committed) {
        while (lexer->queue.length > 0) delete lexer->queue.Shift();
        lexer->offset = offset;
      }
    }

    inline AstNode* Commit(AstNode* result) {
      if (result != NULL) {
        committed = true;
      }
      return result;
    }

   private:
    Lexer* lexer;
    uint32_t offset;
    bool committed;
  };


  inline AstNode* Wrap(AstNode::Type type, AstNode* original) {
    AstNode* wrap = new AstNode(type);
    wrap->children.Push(original);
    return wrap;
  }


  AstNode* Execute();
  AstNode* ParseStatement();
  AstNode* ParseExpression();
  AstNode* ParsePrefixUnop(AstNode::Type type);
  AstNode* ParseBinOp(TokenType type, AstNode* lhs);
  AstNode* ParsePrimary();
  AstNode* ParseMember();
  AstNode* ParseBlock();
  AstNode* ParseScope();

  AstNode* ast;
  AstNode* current;
};

} // namespace dotlang

#endif // _SRC_PARSER_H_
