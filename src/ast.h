#ifndef _SRC_AST_H_
#define _SRC_AST_H_

#include "zone.h" // ZoneObject
#include "utils.h" // List
#include "lexer.h" // lexer
#include "scope.h" // Scope

#include <assert.h> // assert

namespace candor {
namespace internal {

// Forward declaration
struct ScopeSlot;
class AstNode;
class AstValue;

// Just to simplify future use cases
typedef List<AstNode*, ZoneObject> AstList;

#define TYPE_MAPPING_NORMAL(V)\
    V(kBlock)\
    V(kObjectLiteral)\
    V(kArrayLiteral)\
    V(kMember)\
    V(kValue)\
    V(kMValue)\
    V(kProperty)\
    V(kAssign)\
    V(kIf)\
    V(kWhile)\
    V(kReturn)\
    V(kFunction)\
    V(kCall)\
    V(kSelf)\
    V(kUnOp)\
    V(kBinOp)

#define TYPE_MAPPING_LEXER(V)\
    V(kName)\
    V(kNumber)\
    V(kString)\
    V(kTrue)\
    V(kFalse)\
    V(kBreak)\
    V(kContinue)\
    V(kNew)\
    V(kDelete)\
    V(kTypeof)\
    V(kSizeof)\
    V(kKeysof)\
    V(kNil)

// Base class
class AstNode : public ZoneObject {
 public:
  enum Type {

#define MAP_DF(x) x,
    TYPE_MAPPING_NORMAL(MAP_DF)
    TYPE_MAPPING_LEXER(MAP_DF)
#undef MAP_DF

    kNop
  };

  AstNode(Type type) : type_(type),
                       value_(NULL),
                       offset_(-1),
                       length_(0),
                       stack_count_(0),
                       context_count_(0),
                       root_(false) {
  }

  AstNode(Type type, Lexer::Token* token) : type_(type),
                                            value_(token->value()),
                                            offset_(token->offset()),
                                            length_(token->length()),
                                            stack_count_(0),
                                            context_count_(0),
                                            root_(false) {
  }

  virtual ~AstNode() {
  }

  // Converts lexer's token type to ast node type if possible
  static inline Type ConvertType(Lexer::TokenType type) {
    switch (type) {
#define MAP_DF(x) case Lexer::x: return x;
      TYPE_MAPPING_LEXER(MAP_DF)
#undef MAP_DF
     default:
      return kNop;
    }
  }

  // Some shortcuts
  inline AstList* children() { return &children_; }
  inline AstNode* lhs() { return children()->head()->value(); }
  inline AstNode* rhs() { return children()->head()->next()->value(); }

  inline Type type() { return type_; }
  inline void type(Type type) { type_ = type; }
  inline bool is(Type type) { return type_ == type; }

  inline bool is_root() { return root_; }
  inline void make_root() { root_ = true; }

  inline void value(const char* value) { value_ = value; }
  inline const char* value() { return value_; }

  inline void offset(uint32_t offset) { offset_ = offset; }
  inline uint32_t offset() { return offset_; }

  inline void length(uint32_t length) { length_ = length; }
  inline uint32_t length() { return length_; }

  inline void end(uint32_t pos) { length(pos - offset()); }

  inline int32_t stack_slots() { return stack_count_; }
  inline int32_t context_slots() { return context_count_; }

  // Some node (such as Functions) have context and stack variables
  // SetScope will save that information for future uses in generation
  inline void SetScope(Scope* scope) {
    stack_count_ = scope->stack_count();
    context_count_ = scope->context_count();
  }

  bool PrintChildren(PrintBuffer* p, AstList* children) {
    AstList::Item* item = children->head();
    while (item != NULL) {
      if (!item->value()->Print(p)) return false;
      item = item->next();
      if (item != NULL && !p->Print(" ")) return false;
    }

    return true;
  }

  virtual bool Print(PrintBuffer* p) {
    const char* strtype;
    switch (type()) {
#define MAP_DF(x) case x: strtype = #x; break;
      TYPE_MAPPING_NORMAL(MAP_DF)
      TYPE_MAPPING_LEXER(MAP_DF)
#undef MAP_DF
     default:
      strtype = "kNop";
      break;
    }

    return (is(kName) || is(kTrue) || is(kFalse) ||
            is(kNumber) || is(kNil) || is(kBreak) ||
            is(kContinue) || is(kReturn) || is(kIf) ?
               p->Print("[")
               :
               p->Print("[%s ", strtype)
           ) &&

           (length_ == 0 ||
           (p->PrintValue(value(), length()) &&
           (children()->length() == 0 || p->Print(" ")))) &&

           PrintChildren(p, children()) &&
           p->Print("]");
  }

 protected:
  Type type_;

  const char* value_;
  uint32_t offset_;
  uint32_t length_;

  int32_t stack_count_;
  int32_t context_count_;

  bool root_;

  AstList children_;
};
#undef TYPE_MAPPING_NORMAL
#undef TYPE_MAPPING_LEXER


#define TYPE_MAPPING_BINOP(V)\
    V(kAdd)\
    V(kSub)\
    V(kDiv)\
    V(kMul)\
    V(kMod)\
    V(kUShr)\
    V(kShl)\
    V(kShr)\
    V(kBAnd)\
    V(kBOr)\
    V(kBXor)\
    V(kEq)\
    V(kStrictEq)\
    V(kNe)\
    V(kStrictNe)\
    V(kLt)\
    V(kGt)\
    V(kLe)\
    V(kGe)\
    V(kLOr)\
    V(kLAnd)

class BinOp : public AstNode {
 public:
  enum BinOpType {
#define MAP_DF(x) x,
    TYPE_MAPPING_BINOP(MAP_DF)
#undef MAP_DF
    kNone
  };

  BinOp(BinOpType type, AstNode* lhs, AstNode* rhs) : AstNode(kBinOp),
                                                      subtype_(type) {
    children()->Push(lhs);
    children()->Push(rhs);
  }

  static inline BinOp* Cast(AstNode* node) {
    return reinterpret_cast<BinOp*>(node);
  }

  // Converts lexer's token type to binop node type if possible
  static inline BinOpType ConvertType(Lexer::TokenType type) {
    switch (type) {
#define MAP_DF(x) case Lexer::x: return x;
      TYPE_MAPPING_BINOP(MAP_DF)
#undef MAP_DF
     default:
      return kNone;
    }
  }

  bool Print(PrintBuffer* p) {
    const char* strtype;
    switch (subtype()) {
#define MAP_DF(x) case x: strtype = #x; break;
      TYPE_MAPPING_BINOP(MAP_DF)
#undef MAP_DF
     default:
      strtype = "kNone";
      break;
    }

    return p->Print("[%s ", strtype) &&
           PrintChildren(p, children()) &&
           p->Print("]");
  }

  static inline bool is_math(BinOpType type) {
    return type == kAdd || type == kSub || type == kDiv || type == kMul;
  }

  static inline bool is_binary(BinOpType type) {
    return type == kBAnd || type == kBOr || type == kBXor || type == kMod ||
           type == kShl || type == kShr || type == kUShr;
  }

  static inline bool is_logic(BinOpType type) {
    return type == kEq || type == kStrictEq || type == kNe ||
           type == kStrictNe || type == kLt || type == kGt || type == kLe ||
           type == kGe;
  }

  static inline bool is_equality(BinOpType type) {
    return type == kEq || type == kStrictEq ||
           type == kNe || type == kStrictNe;
  }

  static inline bool is_strict_eq(BinOpType type) {
    return type == kStrictEq || type == kStrictNe;
  }

  static inline bool is_negative_eq(BinOpType type) {
    return type == kNe || type == kStrictNe;
  }

  static inline bool is_bool_logic(BinOpType type) {
    return type == kLOr || type == kLAnd;
  }

  static inline bool NumToCompare(BinOpType type, int num) {
    assert(is_logic(type));
    switch (type) {
     case kLt: return num == -1;
     case kGt: return num == 1;
     case kLe: return num <= 0;
     case kGe: return num >= 0;
     case kStrictEq: case kEq: return num == 0;
     case kStrictNe: case kNe: return num != 0;
     default:
      UNEXPECTED
    }

    return false;
  }

  inline BinOpType subtype() { return subtype_; }

 protected:
  BinOpType subtype_;
};

#undef TYPE_MAPPING_BINOP


#define TYPE_MAPPING_UNOP(V)\
    V(kPreInc)\
    V(kPreDec)\
    V(kPostInc)\
    V(kPostDec)\
    V(kNot)\
    V(kPlus)\
    V(kMinus)

class UnOp : public AstNode {
 public:
  enum UnOpType {
#define MAP_DF(x) x,
    TYPE_MAPPING_UNOP(MAP_DF)
#undef MAP_DF
    kNone
  };

  UnOp(UnOpType type, AstNode* expr) : AstNode(kUnOp), subtype_(type) {
    children()->Push(expr);
  }

  static inline UnOp* Cast(AstNode* node) {
    return reinterpret_cast<UnOp*>(node);
  }

  // Converts lexer's token type to unop node type if possible
  static inline UnOpType ConvertPrefixType(Lexer::TokenType type) {
    switch (type) {
     case Lexer::kAdd:
      return kPlus;
     case Lexer::kSub:
      return kMinus;
     case Lexer::kNot:
      return kNot;
     case Lexer::kInc:
      return kPreInc;
     case Lexer::kDec:
      return kPreDec;
     default:
      UNEXPECTED
    }
    return kNone;
  }

  bool Print(PrintBuffer* p) {
    const char* strtype;
    switch (subtype()) {
#define MAP_DF(x) case x: strtype = #x; break;
      TYPE_MAPPING_UNOP(MAP_DF)
#undef MAP_DF
     default:
      strtype = "kNone";
      break;
    }

    return p->Print("[%s ", strtype) &&
           PrintChildren(p, children()) &&
           p->Print("]");
  }

  inline bool is_changing() {
    return subtype_ == kPreInc || subtype_ == kPostInc ||
           subtype_ == kPreDec || subtype_ == kPostDec;
  }
  inline UnOpType subtype() { return subtype_; }

 protected:
  UnOpType subtype_;
};

#undef TYPE_MAPPING_UNOP


class ObjectLiteral : public AstNode {
 public:
  ObjectLiteral() : AstNode(kObjectLiteral) {
  }

  static inline ObjectLiteral* Cast(AstNode* node) {
    return reinterpret_cast<ObjectLiteral*>(node);
  }

  bool Print(PrintBuffer* p) {
    if (!p->Print("[kObjectLiteral ")) return false;

    AstList::Item* key = keys()->head();
    AstList::Item* value = values()->head();

    while (key != NULL) {
      if (!key->value()->Print(p) || !p->Print(":") ||
          !value->value()->Print(p) ||
          (key->next() != NULL && !p->Print(" "))) {
        return false;
      }
      key = key->next();
      value = value->next();
    }

    return p->Print("]");
  }

  inline AstList* keys() { return &keys_; }
  inline AstList* values() { return children(); }

 protected:
  AstList keys_;
};


// Specific AST node for function,
// contains name and variables list
class FunctionLiteral : public AstNode {
 public:
  FunctionLiteral(AstNode* variable) : AstNode(kFunction) {
    if (variable != NULL) {
      offset(variable->offset());
      length(variable->length());
    }

    variable_ = variable;
  }

  static inline FunctionLiteral* Cast(AstNode* node) {
    return reinterpret_cast<FunctionLiteral*>(node);
  }

  inline bool CheckDeclaration() {
    // Function without body is a call
    if (children()->length() == 0) {
      // So it should have a name
      if (variable() == NULL) return false;

      // Transform node into a call
      type(kCall);
      return true;
    }

    // Name should not be "a.b.c"
    if (variable() != NULL && !variable()->is(kName)) return false;

    // Arguments should be a kName, not expressions
    AstList::Item* head;
    for (head = args_.head(); head != NULL; head = head->next()) {
      if (!head->value()->is(kName)) return false;
    }

    return true;
  }


  bool Print(PrintBuffer* p) {
    return p->Print(type() == kFunction ? "[kFunction " : "[kCall ") &&
           (variable() == NULL ?
               p->Print("(anonymous)")
               :
               variable()->Print(p)
           ) &&
           p->Print(" @[") &&
           PrintChildren(p, args()) &&
           p->Print("] ") &&
           PrintChildren(p, children()) &&
           p->Print("]");
  }

  inline AstNode* variable() { return variable_; }
  inline void variable(AstNode* variable) { variable_ = variable; }
  inline AstList* args() { return &args_; }

  AstNode* variable_;
  AstList args_;

  uint32_t offset_;
  uint32_t length_;
};


// Every kName AST node will be replaced by
// AST value with scope information
// (is variable on-stack or in-context, it's index, and etc)
class AstValue : public AstNode {
 public:
  enum AstValueType {
    kSlot,
    kOperand,
    kSpill
  };

  AstValue(Scope* scope, AstNode* name) : AstNode(kValue),
                                          type_(kSlot),
                                          name_(name) {
    slot_ = scope->GetSlot(name->value(), name->length());
  }

  AstValue(ScopeSlot* slot, AstNode* name) : AstNode(kValue),
                                             type_(kSlot),
                                             slot_(slot),
                                             name_(name) {
  }

  AstValue(AstValueType type) : AstNode(kValue), type_(type) {
  }

  static inline AstValue* Cast(AstNode* node) {
    return reinterpret_cast<AstValue*>(node);
  }

  bool Print(PrintBuffer* p) {
    assert(!is_spill() && !is_operand());

    return p->Print("[") &&
           p->PrintValue(name()->value(), name()->length()) &&
           (slot()->is_context() ?
              p->Print(" @context[%d]:%d", slot()->depth(), slot()->index())
              :
              p->Print(" @stack:%d", slot()->index())
           ) &&
           p->Print("]");
  }

  inline bool is_slot() { return type_ == kSlot; }
  inline bool is_operand() { return type_ == kOperand; }
  inline bool is_spill() { return type_ == kSpill; }

  inline ScopeSlot* slot() { return slot_; }
  inline AstNode* name() { return name_; }

 protected:
  AstValueType type_;
  ScopeSlot* slot_;
  AstNode* name_;
};

} // namespace internal
} // namespace candor

#endif // _SRC_AST_H_
