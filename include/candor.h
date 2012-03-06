#ifndef _INCLUDE_CANDOR_H_
#define _INCLUDE_CANDOR_H_

#include <stdint.h> // uint32_t

namespace candor {

// Forward declarations
namespace internal {
  class Heap;
  class CodeSpace;
} // namespace internal

class Value;
class Function;
class Number;
class Boolean;
class String;
class Object;

class Isolate {
 public:
  Isolate();
  ~Isolate();

 protected:
  internal::Heap* heap;
  internal::CodeSpace* space;

  friend class Value;
  friend class Function;
  friend class Number;
  friend class Boolean;
  friend class String;
  friend class Object;
};

class Value {
 public:
  enum ValueType {
    kNil,
    kNumber,
    kBoolean,
    kString,
    kFunction,
    kObject
  };

  static Value* New(char* addr);

  template <class T>
  T* As();

  template <class T>
  static T* Cast(char* addr);

  template <class T>
  bool Is();

  inline char* addr() { return reinterpret_cast<char*>(this); }
};

class Function : public Value {
 public:
  static Function* New(Isolate* isolate, const char* source, uint32_t length);

  Value* Call(Isolate* isolate, Object* context, uint32_t argc, Value* argv[]);

  static const ValueType tag = kFunction;
};

class Nil : public Value {
 public:
  static Nil* New();

  static const ValueType tag = kNil;
};

class Boolean : public Value {
 public:
  static Boolean* True(Isolate* isolate);
  static Boolean* False(Isolate* isolate);

  bool IsTrue();
  bool IsFalse();

  static const ValueType tag = kBoolean;
};

class Number : public Value {
 public:
  static Number* New(Isolate* isolate, double value);
  static Number* New(Isolate* isolate, int64_t value);

  double Value();
  int64_t IntegralValue();

  bool IsIntegral();

  static const ValueType tag = kNumber;
};

class String : public Value {
 public:
  static String* New(Isolate* isolate, const char* value, uint32_t len);

  const char* Value();
  uint32_t Length();

  static const ValueType tag = kString;
};

class Object : public Value {
 public:
  static Object* New(Isolate* isolate);

  void Set(const char* key, uint32_t len, Value* value);
  Value* Get(const char* key, uint32_t len);

  static const ValueType tag = kObject;
};

} // namespace candor

#endif // _INCLUDE_CANDOR_H_
