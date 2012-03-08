#ifndef _INCLUDE_CANDOR_H_
#define _INCLUDE_CANDOR_H_

#include "utils.h" // List
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
class HandleScope;
class Arguments;

class Isolate {
 public:
  Isolate();
  ~Isolate();

  static Isolate* GetCurrent();

 protected:
  internal::Heap* heap;
  internal::CodeSpace* space;

  HandleScope* scope;

  friend class Value;
  friend class Function;
  friend class Number;
  friend class Boolean;
  friend class String;
  friend class Object;

  friend class HandleScope;
  template <class T>
  friend class Handle;
};

class Value {
 public:
  enum ValueType {
    kNone,
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
  static T* Cast(Value* value);

  template <class T>
  bool Is();

  inline char* addr() { return reinterpret_cast<char*>(this); }

  static const ValueType tag = kNone;
};

class Function : public Value {
 public:
  typedef Value* (*BindingCallback)(uint32_t argc, Arguments& argv);

  static Function* New(const char* source, uint32_t length);
  static Function* New(BindingCallback callback);

  Value* Call(Object* context, uint32_t argc, Value* argv[]);

  static const ValueType tag = kFunction;
};

class Nil : public Value {
 public:
  static Nil* New();

  static const ValueType tag = kNil;
};

class Boolean : public Value {
 public:
  static Boolean* True();
  static Boolean* False();

  bool IsTrue();
  bool IsFalse();

  static const ValueType tag = kBoolean;
};

class Number : public Value {
 public:
  static Number* NewDouble(double value);
  static Number* NewIntegral(int64_t value);

  double Value();
  int64_t IntegralValue();

  bool IsIntegral();

  static const ValueType tag = kNumber;
};

class String : public Value {
 public:
  static String* New(const char* value, uint32_t len);

  const char* Value();
  uint32_t Length();

  static const ValueType tag = kString;
};

class Object : public Value {
 public:
  static Object* New();

  void Set(String* key, Value* value);
  Value* Get(String* key);

  static const ValueType tag = kObject;
};

class Arguments {
 public:
  Value* operator[] (const int index);
};

template <class T>
class Handle {
 public:
  Handle(T* v);

  void Persist();
  void Weaken();

  inline T* operator*() { return value; }
  inline T* operator->() { return value; }

  inline T** addr() { return &value; }

  template <class S>
  inline static Handle<T> Cast(Handle<S> handle) {
    return Handle<T>(Value::Cast<T>(*handle));
  }

 protected:
  Isolate* isolate;
  T* value;
};

class HandleScope {
 public:
  HandleScope();
  ~HandleScope();

  typedef internal::List<Value*, internal::EmptyClass> ValueList;

  void Put(Handle<Value> handle);

 private:
  Isolate* isolate;
  HandleScope* parent;

  ValueList references;
};

} // namespace candor

#endif // _INCLUDE_CANDOR_H_
