#include "parser.h"
#include "ast.h"
#include <assert.h> // assert
#include <stdlib.h> // NULL

namespace dotlang {

AstNode* Parser::Execute() {
  AstNode* stmt;
  while ((stmt = ParseStatement()) != NULL) {
    ast->children.Push(stmt);
  }
  assert(Peek()->type == kEnd);

  return ast;
}


AstNode* Parser::ParseStatement() {
  Position pos(this);
  AstNode* result = NULL;

  // Skip cr's before statement
  // needed for {\n blocks \n}
  while (Peek()->type == kCr) Skip();

  switch (Peek()->type) {
   case kReturn:
    Skip();
    {
      AstNode* retValue = NULL;
      if (Peek()->type != kEnd && Peek()->type != kCr) {
        retValue = ParseExpression();
      }
      result = new AstNode(AstNode::kReturn);
      if (retValue != NULL) result->children.Push(retValue);
    }
    break;
   case kBreak:
    Skip();
    result = new AstNode(AstNode::kBreak);
    break;
   case kIf:
    Skip();
    {
      if (Peek()->type != kParenOpen) break;
      Skip();

      AstNode* cond = ParseExpression();
      if (cond == NULL) break;

      if (Peek()->type != kParenClose) {
        delete cond;
        break;
      }
      Skip();

      AstNode* body = ParseBlock();
      AstNode* elseBody = NULL;

      if (Peek()->type == kElse) {
        Skip();
        elseBody = ParseBlock();
      }

      IfStmt* w = new IfStmt(cond, body, elseBody);
      result = w;
    }
    break;
   case kWhile:
    Skip();
    {
      if (Peek()->type != kParenOpen) break;
      Skip();

      AstNode* cond = ParseExpression();
      if (cond == NULL) break;
      if (Peek()->type != kParenClose) {
        delete cond;
        break;
      }
      Skip();

      AstNode* body = ParseBlock();

      WhileStmt* w = new WhileStmt(cond, body);
      result = w;
    }
    break;
   case kBraceOpen:
    result = ParseBlock();
    break;
   default:
    result = ParseExpression();
    break;
  }

  // Consume kCr or kBraceClose
  if (Peek()->type != kEnd &&
      Peek()->type != kCr &&
      Peek()->type != kBraceClose) {
    delete result;
    return NULL;
  }
  Skip();

  return pos.Commit(result);
}


#define BINOP_PRI1\
    case kEq:\
    case kNe:\
    case kStrictEq:\
    case kStrictNe:

#define BINOP_PRI2\
    case kLt:\
    case kGt:\
    case kLe:\
    case kGe:

#define BINOP_PRI3\
    case kLOr:\
    case kLAnd:

#define BINOP_PRI4\
    case kBOr:\
    case kBAnd:\
    case kBXor:

#define BINOP_PRI5\
    case kAdd:\
    case kSub:

#define BINOP_PRI6\
    case kMul:\
    case kDiv:

#define BINOP_SWITCH(type, result, K)\
    type = Peek()->type;\
    switch (type) {\
     K\
      result = ParseBinOp(type, result);\
     default:\
      break;\
    }\
    if (result == NULL) return result;


AstNode* Parser::ParseExpression() {
  AstNode* result = NULL;

  // Parse prefix unops and block expression
  switch (Peek()->type) {
   case kInc:
    return ParsePrefixUnop(AstNode::kPreInc);
   case kDec:
    return ParsePrefixUnop(AstNode::kPreDec);
   case kNot:
    return ParsePrefixUnop(AstNode::kNot);
   case kBraceOpen:
    result = ParseBlock();
    result->type = AstNode::kBlockExpr;
    return result;
   default:
    break;
  }

  Position pos(this);
  AstNode* member = ParseMember();

  switch (Peek()->type) {
   case kAssign:
    if (member == NULL) return NULL;

    // member "=" expr
    {
      Skip();
      AstNode* value = ParseExpression();
      if (value == NULL) break;
      AssignExpr* expr = new AssignExpr(member, value);
      result = expr;
    }
    break;
   case kParenOpen:
    // member "(" ... args ... ")" block? |
    // member? "(" ... args ... ")" block
    {
      Skip();
      FunctionLiteral* fn = new FunctionLiteral(member);
      member = NULL;

      while (Peek()->type != kParenClose && Peek()->type != kEnd) {
        AstNode* expr = ParseExpression();
        if (expr == NULL) break;
        fn->args.Push(expr);

        // Skip commas
        if (Peek()->type == kComma) Skip();
      }
      if (Peek()->type != kParenClose) {
        delete fn;
        break;
      }
      Skip();

      // Optional body (for function declaration)
      fn->body = ParseBlock();
      if (!fn->CheckDeclaration()) {
        delete fn;
        break;
      }

      result = fn;
    }
    break;
   default:
    result = member;
    break;
  }

  if (result == NULL) {
    delete member;
    return result;
  }

  // Parse postfixes
  TokenType type = Peek()->type;
  switch (type) {
   case kInc:
   case kDec:
    Skip();
    result = Wrap(AstNode::ConvertType(type), result);
    break;
   default:
    break;
  }

  // Parse binops ordered by priority
  BINOP_SWITCH(type, result, BINOP_PRI1)
  BINOP_SWITCH(type, result, BINOP_PRI2)
  BINOP_SWITCH(type, result, BINOP_PRI3)
  BINOP_SWITCH(type, result, BINOP_PRI4)
  BINOP_SWITCH(type, result, BINOP_PRI5)
  BINOP_SWITCH(type, result, BINOP_PRI6)

  return pos.Commit(result);
}


AstNode* Parser::ParsePrefixUnop(AstNode::Type type) {
  Position pos(this);

  // Consume prefix token
  Skip();

  AstNode* expr = ParseExpression();
  if (expr == NULL) return NULL;

  return pos.Commit(Wrap(type, expr));
}


AstNode* Parser::ParseBinOp(TokenType type, AstNode* lhs) {
  Position pos(this);

  // Consume binop token
  Skip();

  AstNode* rhs = ParseExpression();
  if (rhs == NULL) {
    delete lhs;
    return NULL;
  }

  AstNode* result = Wrap(AstNode::ConvertType(type), lhs);
  result->children.Push(rhs);

  return pos.Commit(result);
}


AstNode* Parser::ParsePrimary() {
  Position pos(this);
  Lexer::Token* token = Peek();
  AstNode* result = NULL;

  switch (token->type) {
   case kName:
   case kNumber:
   case kString:
   case kTrue:
   case kFalse:
   case kNil:
    result = new AstNode(AstNode::ConvertType(token->type));
    result->FromToken(token);
    Skip();
    break;
   case kParenOpen:
    Skip();
    result = ParseExpression();
    if (Peek()->type != kParenClose) {
      delete result;
      result = NULL;
    } else {
      Skip();
    }
    break;
   // TODO: implement others
   default:
    result = NULL;
    break;
  }

  return pos.Commit(result);
}


AstNode* Parser::ParseMember() {
  Position pos(this);
  AstNode* result = ParsePrimary();
  if (result == NULL) return NULL;

  while (Peek()->type != kEnd && Peek()->type != kCr) {
    AstNode* next = NULL;
    if (Peek()->type == kDot) {
      // a.b
      Skip();
      next = ParsePrimary();
    } else if (Peek()->type == kArrayOpen) {
      // a["prop-expr"]
      Skip();
      next = ParseExpression();
      if (Peek()->type != kArrayClose) {
        delete next;
        next = NULL;
      } else {
        Skip();
      }
    }
    if (next == NULL) break;

    result = Wrap(AstNode::kMember, result);
    result->children.Push(next);
  }

  return pos.Commit(result);
}


AstNode* Parser::ParseBlock() {
  if (Peek()->type != kBraceOpen) return NULL;

  Position pos(this);
  AstNode* result;
  Skip();
  result = new AstNode(AstNode::kBlock);
  while (Peek()->type != kEnd && Peek()->type != kBraceClose) {
    AstNode* stmt = ParseStatement();
    if (stmt == NULL) break;
    result->children.Push(stmt);
  }
  if (Peek()->type != kEnd && Peek()->type != kBraceClose) {
    delete result;
    return NULL;
  }
  Skip();

  return pos.Commit(result);
}


} // namespace dotlang
