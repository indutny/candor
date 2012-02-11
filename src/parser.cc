#include "parser.h"
#include "scope.h" // Scope
#include "ast.h"
#include <assert.h> // assert
#include <stdlib.h> // NULL

namespace dotlang {

AstNode* Parser::Execute() {
  Scope scope(this, Scope::kFunction);

  AstNode* stmt;
  while ((stmt = ParseStatement()) != NULL) {
    ast()->children()->Push(stmt);
  }
  assert(Peek()->is(kEnd));

  ast()->SetScope(&scope);

  return ast();
}


AstNode* Parser::ParseStatement() {
  Position pos(this);
  AstNode* result = NULL;

  // Skip cr's before statement
  // needed for {\n blocks \n}
  while (Peek()->is(kCr)) Skip();

  switch (Peek()->type()) {
   case kReturn:
    Skip();
    {
      AstNode* retValue = NULL;
      if (!Peek()->is(kEnd) && !Peek()->is(kCr)) {
        retValue = ParseExpression();
      }
      result = new AstNode(AstNode::kReturn);
      if (retValue != NULL) result->children()->Push(retValue);
    }
    break;
   case kBreak:
    Skip();
    result = new AstNode(AstNode::kBreak);
    break;
   case kIf:
    Skip();
    {
      if (!Peek()->is(kParenOpen)) break;
      Skip();

      AstNode* cond = ParseExpression();
      if (cond == NULL) break;

      if (!Peek()->is(kParenClose)) break;
      Skip();

      AstNode* body = ParseBlock(NULL);
      AstNode* elseBody = NULL;

      if (body == NULL) {
        body = ParseStatement();
      } else {
        if (Peek()->is(kElse)) {
          Skip();
          elseBody = ParseBlock(NULL);
        }
      }

      if (body == NULL) return NULL;

      result = new AstNode(AstNode::kIf);
      result->children()->Push(cond);
      result->children()->Push(body);
      if (elseBody != NULL) result->children()->Push(elseBody);
    }
    break;
   case kWhile:
    Skip();
    {
      if (!Peek()->is(kParenOpen)) break;
      Skip();

      AstNode* cond = ParseExpression();
      if (cond == NULL) break;
      if (!Peek()->is(kParenClose)) break;
      Skip();

      AstNode* body = ParseBlock(NULL);

      result = new AstNode(AstNode::kWhile);
      result->children()->Push(cond);
      result->children()->Push(body);
    }
    break;
   case kBraceOpen:
    result = ParseBlock(NULL);
    break;
   default:
    result = ParseExpression();
    break;
  }

  // Consume kCr or kBraceClose
  if (!Peek()->is(kEnd) && !Peek()->is(kCr) && !Peek()->is(kBraceClose)) {
    return NULL;
  }
  if (Peek()->is(kCr)) Skip();

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
    type = Peek()->type();\
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
  switch (Peek()->type()) {
   case kInc:
    return ParsePrefixUnop(AstNode::kPreInc);
   case kDec:
    return ParsePrefixUnop(AstNode::kPreDec);
   case kNot:
    return ParsePrefixUnop(AstNode::kNot);
   case kBraceOpen:
    result = ParseBlock(NULL);
    if (result == NULL) return NULL;
    result->type_ = AstNode::kBlockExpr;
    return result;
   default:
    break;
  }

  Position pos(this);
  AstNode* member = ParseMember();

  switch (Peek()->type()) {
   case kAssign:
    if (member == NULL) return NULL;

    // member "=" expr
    {
      Skip();
      AstNode* value = ParseExpression();
      if (value == NULL) break;
      result = new AstNode(AstNode::kAssign);
      result->children()->Push(member);
      result->children()->Push(value);
    }
    break;
   case kParenOpen:
    // member "(" ... args ... ")" block? |
    // member? "(" ... args ... ")" block
    {
      FunctionLiteral* fn = new FunctionLiteral(member, Peek()->offset());
      Skip();
      member = NULL;

      while (!Peek()->is(kParenClose) && !Peek()->is(kEnd)) {
        AstNode* expr = ParseExpression();
        if (expr == NULL) break;
        fn->args()->Push(expr);

        // Skip commas
        if (Peek()->is(kComma)) Skip();
      }
      if (!Peek()->is(kParenClose)) break;
      Skip();

      // Optional body (for function declaration)
      ParseBlock(reinterpret_cast<AstNode*>(fn));
      if (!fn->CheckDeclaration()) break;

      result = fn->End(Peek()->offset());
    }
    break;
   default:
    result = member;
    break;
  }

  if (result == NULL) return result;

  // Parse postfixes
  TokenType type = Peek()->type();
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
  if (rhs == NULL) return NULL;

  AstNode* result = Wrap(AstNode::ConvertType(type), lhs);
  result->children()->Push(rhs);

  return pos.Commit(result);
}


AstNode* Parser::ParsePrimary() {
  Position pos(this);
  Lexer::Token* token = Peek();
  AstNode* result = NULL;

  switch (token->type()) {
   case kName:
   case kNumber:
   case kString:
   case kTrue:
   case kFalse:
   case kNil:
    result = new AstNode(AstNode::ConvertType(token->type()));
    result->FromToken(token);
    Skip();
    break;
   case kParenOpen:
    Skip();
    result = ParseExpression();
    if (!Peek()->is(kParenClose)) {
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

  // Lookup names in scope
  if (result->type() == AstNode::kName) {
    result = new AstValue(scope(), result);
  }

  while (!Peek()->is(kEnd) && !Peek()->is(kCr)) {
    AstNode* next = NULL;
    if (Peek()->is(kDot)) {
      // a.b
      Skip();
      next = ParsePrimary();
    } else if (Peek()->is(kArrayOpen)) {
      // a["prop-expr"]
      Skip();
      next = ParseExpression();
      if (Peek()->is(kArrayClose)) {
        Skip();
      } else {
        next = NULL;
      }
    }
    if (next == NULL) break;

    result = Wrap(AstNode::kMember, result);
    result->children()->Push(next);
  }

  return pos.Commit(result);
}


AstNode* Parser::ParseBlock(AstNode* block) {
  if (!Peek()->is(kBraceOpen)) return NULL;

  bool fn = block != NULL;

  Position pos(this);
  Scope scope(this, fn ? Scope::kFunction : Scope::kBlock);

  Skip();

  while (Peek()->is(kCr)) Skip();
  AstNode* result = fn ? block : new AstNode(AstNode::kBlock);

  result->children()->Push(ParseScope());

  while (!Peek()->is(kEnd) && !Peek()->is(kBraceClose)) {
    AstNode* stmt = ParseStatement();
    if (stmt == NULL) break;
    result->children()->Push(stmt);
  }
  if (!Peek()->is(kEnd) && !Peek()->is(kBraceClose)) return NULL;
  Skip();

  if (fn) result->SetScope(&scope);

  return pos.Commit(result);
}


AstNode* Parser::ParseScope() {
  if (!Peek()->is(kScope)) return NULL;
  Position pos(this);

  Skip();

  AstNode* result = new AstNode(AstNode::kScopeDecl);

  while (!Peek()->is(kCr) && !Peek()->is(kBraceClose)) {
    if (!Peek()->is(kName)) break;

    AstNode* name = (new AstNode(AstNode::kName))->FromToken(Peek());
    scope()->MoveToContext(name->value_, name->length_);

    result->children()->Push(name);
    Skip();

    if (!Peek()->is(kComma) && !Peek()->is(kCr) && !Peek()->is(kBraceClose)) {
      break;
    }
    if (Peek()->is(kComma)) Skip();
  }

  if (!Peek()->is(kCr) && !Peek()->is(kBraceClose)) {
    result = NULL;
  }

  return pos.Commit(result);
}

} // namespace dotlang
