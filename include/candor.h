#ifndef _INCLUDE_CANDOR_H_
#define _INCLUDE_CANDOR_H_

#include <stdint.h> // uint32_t
#include <sys/types.h> // size_t

namespace candor {

// Forward declarations
namespace internal {
  class Heap;
  class CodeSpace;
  template <class T, class ItemParent>
  class List;
  class EmptyClass;
} // namespace internal

class Value;
class Function;
class Number;
class Boolean;
class String;
class Object;
class Array;
class CData;
class Arguments;
struct SyntaxError;

class Isolate {
 public:
  Isolate();
  ~Isolate();

  static Isolate* GetCurrent();

  bool HasSyntaxError();
  SyntaxError* GetSyntaxError();
  void PrintSyntaxError();

 protected:

  void SetSyntaxError(SyntaxError* err);

  internal::Heap* heap;
  internal::CodeSpace* space;

  SyntaxError* syntax_error;

  friend class Value;
  friend class Function;
  friend class Number;
  friend class Boolean;
  friend class String;
  friend class Object;
  friend class Array;
  friend class CData;

  template <class T>
  friend class Handle;
};

struct SyntaxError {
  const char* message;
  int line;
  int offset;

  const char* source;
  uint32_t length;
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
    kObject,
    kArray,
    kCData
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

  Number* ToNumber();
  Boolean* ToBoolean();
  String* ToString();

  inline char* addr() { return reinterpret_cast<char*>(this); }

  static const ValueType tag = kNone;
};

class Function : public Value {
 public:
  typedef Value* (*BindingCallback)(uint32_t argc, Arguments& argv);

  static Function* New(const char* source, uint32_t length);
  static Function* New(BindingCallback callback);

  Object* GetContext();
  void SetContext(Object* context);

  Value* Call(uint32_t argc, Value* argv[]);

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
  static String* New(const char* value);
  static String* New(const char* value, uint32_t len);

  const char* Value();
  uint32_t Length();

  static const ValueType tag = kString;
};

class Object : public Value {
 public:
  static Object* New();

  void Set(String* key, Value* value);
  void Set(const char* key, Value* value);
  Value* Get(String* key);
  Value* Get(const char* key);

  static const ValueType tag = kObject;
};

class Array : public Value {
 public:
  static Array* New();

  void Set(int64_t key, Value* value);
  Value* Get(int64_t key);
  int64_t Length();

  static const ValueType tag = kArray;
};

class CData : public Value {
 public:
  static CData* New(size_t size);

  void* GetContents();

  static const ValueType tag = kCData;
};

class Arguments {
 public:
  Value* operator[] (const int index);
};

template <class T>
class Handle {
 public:
  Handle(Value* v);
  ~Handle();

  typedef void (*WeakCallback)(T* value);

  void SetWeakCallback(WeakCallback callback);
  void ClearWeak();

  inline T* operator*() { return value; }
  inline T* operator->() { return value; }

  template <class S>
  static inline Handle<T> Cast(Handle<S> handle) {
    return Handle<T>(Value::Cast<T>(*handle));
  }

 protected:
  Isolate* isolate;
  T* value;
};

class CWrapper {
 public:
  CWrapper();
  virtual ~CWrapper();

  inline CData* Wrap() { return data; }

  template <class T>
  static inline T* Unwrap(Value* value) {
    return *reinterpret_cast<T**>(value->As<CData>()->GetContents());
  }

  void Ref();
  void Unref();

  static void WeakCallback(CData* data);

 private:
  CData* data;

  int ref_count;
  Handle<CData>* ref;
};

} // namespace candor

#endif // _INCLUDE_CANDOR_H_
