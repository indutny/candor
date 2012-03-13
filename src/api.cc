#include "candor.h"
#include "heap.h"
#include "heap-inl.h"
#include "code-space.h"
#include "runtime.h"
#include "utils.h"

#include <stdio.h> // fprintf
#include <stdint.h> // uint32_t
#include <string.h> // strlen
#include <stdlib.h> // NULL

namespace candor {

using namespace internal;

// Declarations
#define TYPES_LIST(V)\
    V(Value)\
    V(Nil)\
    V(Number)\
    V(Boolean)\
    V(String)\
    V(Function)\
    V(Object)\
    V(Array)\
    V(CData)

#define METHODS_ENUM(V)\
    template V* Value::As<V>();\
    template V* Value::Cast<V>(char* addr);\
    template V* Value::Cast<V>(Value* value);\
    template bool Value::Is<V>();\
    template Handle<V>::Handle();\
    template Handle<V>::Handle(Value* v);\
    template Handle<V>::~Handle();\
    template void Handle<V>::Wrap(Value* v);\
    template void Handle<V>::Unwrap();\
    template void Handle<V>::SetWeakCallback(WeakCallback callback);\
    template void Handle<V>::ClearWeak();
TYPES_LIST(METHODS_ENUM)
#undef METHODS_ENUM

#undef TYPES_LIST

static Isolate* current_isolate = NULL;

Isolate::Isolate() {
  heap = new Heap(2 * 1024 * 1024);
  space = new CodeSpace(heap);
  syntax_error = NULL;

  current_isolate = this;
}


Isolate::~Isolate() {
  delete heap;
  delete space;
}


Isolate* Isolate::GetCurrent() {
  // TODO: Support multiple isolates
  return current_isolate;
}


bool Isolate::HasSyntaxError() {
  return syntax_error != NULL;
}


SyntaxError* Isolate::GetSyntaxError() {
  return syntax_error;
}


void Isolate::PrintSyntaxError() {
  if (!HasSyntaxError()) return;

  fprintf(stderr,
          "SyntaxError on line %d: %s\n",
          syntax_error->line,
          syntax_error->message);
}


void Isolate::SetSyntaxError(SyntaxError* err) {
  if (HasSyntaxError()) {
    delete syntax_error;
  }
  syntax_error = err;
}


template <class T>
Handle<T>::Handle() : isolate(Isolate::GetCurrent()), value(NULL) {
}


template <class T>
Handle<T>::Handle(Value* v) : isolate(Isolate::GetCurrent()), value(NULL) {
  Wrap(v);
}


template <class T>
Handle<T>::~Handle() {
  Unwrap();
}


template <class T>
void Handle<T>::Wrap(Value* v) {
  Unwrap();

  value = v->As<T>();
  isolate->heap->Reference(reinterpret_cast<HValue**>(&value),
                           reinterpret_cast<HValue*>(value));
}


template <class T>
void Handle<T>::Unwrap() {
  if (value == NULL) return;
  isolate->heap->Dereference(reinterpret_cast<HValue**>(&value),
                             reinterpret_cast<HValue*>(value));
  value = NULL;
}


template <class T>
void Handle<T>::SetWeakCallback(WeakCallback callback) {
  isolate->heap->AddWeak(reinterpret_cast<HValue*>(value),
                         *reinterpret_cast<Heap::WeakCallback*>(&callback));
}


template <class T>
void Handle<T>::ClearWeak() {
  isolate->heap->RemoveWeak(reinterpret_cast<HValue*>(value));
}


Value* Value::New(char* addr) {
  return reinterpret_cast<Value*>(addr);
}


template <class T>
T* Value::As() {
  assert(Is<T>());
  return reinterpret_cast<T*>(this);
}


template <class T>
T* Value::Cast(char* addr) {
  return reinterpret_cast<T*>(addr);
}


template <class T>
T* Value::Cast(Value* value) {
  assert(value->Is<T>());
  return reinterpret_cast<T*>(value);
}


template <class T>
bool Value::Is() {
  Heap::HeapTag tag = Heap::kTagNil;

  switch (T::tag) {
   case kNil: tag = Heap::kTagNil; break;
   case kNumber: tag = Heap::kTagNumber; break;
   case kBoolean: tag = Heap::kTagBoolean; break;
   case kString: tag = Heap::kTagString; break;
   case kFunction: tag = Heap::kTagFunction; break;
   case kObject: tag = Heap::kTagObject; break;
   case kArray: tag = Heap::kTagArray; break;
   case kCData: tag = Heap::kTagCData; break;
   default: return false;
  }

  return HValue::GetTag(addr()) == tag;
}


Number* Value::ToNumber() {
  return Cast<Number>(RuntimeToNumber(Isolate::GetCurrent()->heap, addr()));
}


Boolean* Value::ToBoolean() {
  return Cast<Boolean>(RuntimeToBoolean(Isolate::GetCurrent()->heap, addr()));
}


String* Value::ToString() {
  return Cast<String>(RuntimeToString(Isolate::GetCurrent()->heap, addr()));
}


Function* Function::New(const char* source, uint32_t length) {
  char* root;
  SyntaxError* error;
  char* code = Isolate::GetCurrent()->space->Compile(source,
                                                     length,
                                                     &root,
                                                     &error);
  // Set errors
  if (code == NULL) {
    Isolate::GetCurrent()->SetSyntaxError(error);
    return NULL;
  } else {
    Isolate::GetCurrent()->SetSyntaxError(NULL);
  }

  char* obj = HFunction::New(Isolate::GetCurrent()->heap, NULL, code, root);

  Function* fn = Cast<Function>(obj);
  Isolate::GetCurrent()->heap->Reference(NULL,
                                         reinterpret_cast<HValue*>(fn));

  return fn;
}


Function* Function::New(BindingCallback callback) {
  char* obj = HFunction::NewBinding(Isolate::GetCurrent()->heap,
                                    *reinterpret_cast<char**>(&callback),
                                    NULL);

  Function* fn = Cast<Function>(obj);
  Isolate::GetCurrent()->heap->Reference(NULL,
                                         reinterpret_cast<HValue*>(fn));

  return fn;
}


Object* Function::GetContext() {
  return Cast<Object>(HFunction::GetContext(addr()));
}


void Function::SetContext(Object* context) {
  return HFunction::SetContext(addr(), context->addr());
}


Value* Function::Call(uint32_t argc, Value* argv[]) {
  return Isolate::GetCurrent()->space->Run(addr(), argc, argv);
}


Nil* Nil::New() {
  return NULL;
}


Boolean* Boolean::True() {
  return Cast<Boolean>(HBoolean::New(Isolate::GetCurrent()->heap,
                                     Heap::kTenureNew,
                                     true));
}


Boolean* Boolean::False() {
  return Cast<Boolean>(HBoolean::New(Isolate::GetCurrent()->heap,
                                     Heap::kTenureNew,
                                     false));
}


bool Boolean::IsTrue() {
  return HBoolean::Value(addr());
}


bool Boolean::IsFalse() {
  return !HBoolean::Value(addr());
}


Number* Number::NewDouble(double value) {
  return Cast<Number>(HNumber::New(
        Isolate::GetCurrent()->heap, Heap::kTenureNew, value));
}


Number* Number::NewIntegral(int64_t value) {
  return Cast<Number>(HNumber::New(Isolate::GetCurrent()->heap, value));
}


double Number::Value() {
  return HNumber::DoubleValue(addr());
}


int64_t Number::IntegralValue() {
  return HNumber::IntegralValue(addr());
}


bool Number::IsIntegral() {
  return HNumber::IsIntegral(addr());
}


String* String::New(const char* value, uint32_t len) {
  return Cast<String>(HString::New(
        Isolate::GetCurrent()->heap, Heap::kTenureNew, value, len));
}


String* String::New(const char* value) {
  return Cast<String>(HString::New(
        Isolate::GetCurrent()->heap, Heap::kTenureNew, value, strlen(value)));
}


const char* String::Value() {
  return HString::Value(addr());
}


uint32_t String::Length() {
  return HString::Length(addr());
}


Object* Object::New() {
  return Cast<Object>(HObject::NewEmpty(Isolate::GetCurrent()->heap));
}


void Object::Set(String* key, Value* value) {
  char** slot = reinterpret_cast<char**>(RuntimeLookupProperty(
        Isolate::GetCurrent()->heap, addr(), key->addr(), 1));
  *slot = value->addr();
}


Value* Object::Get(String* key) {
  char** slot = reinterpret_cast<char**>(RuntimeLookupProperty(
        Isolate::GetCurrent()->heap, addr(), key->addr(), 0));

  return Value::New(*slot);
}


void Object::Set(const char* key, Value* value) {
  return Set(String::New(key), value);
}


Value* Object::Get(const char* key) {
  return Get(String::New(key));
}


Array* Array::New() {
  return Cast<Array>(HArray::NewEmpty(Isolate::GetCurrent()->heap));
}


void Array::Set(int64_t key, Value* value) {
  char* keyptr = reinterpret_cast<char*>(HNumber::Tag(key));
  char** slot = reinterpret_cast<char**>(RuntimeLookupProperty(
        Isolate::GetCurrent()->heap, addr(), keyptr, 1));
  *slot = value->addr();
}


Value* Array::Get(int64_t key) {
  char* keyptr = reinterpret_cast<char*>(HNumber::Tag(key));
  char** slot = reinterpret_cast<char**>(RuntimeLookupProperty(
        Isolate::GetCurrent()->heap, addr(), keyptr, 1));

  return Value::New(*slot);
}


int64_t Array::Length() {
  return HArray::Length(addr(), true);
}


CData* CData::New(size_t size) {
  return Cast<CData>(HCData::New(Isolate::GetCurrent()->heap, size));
}


void* CData::GetContents() {
  return HCData::Data(addr());
}


CWrapper::CWrapper() : data(CData::New(sizeof(void*))),
                       ref_count(0),
                       ref(NULL) {
  // Save pointer of class
  *reinterpret_cast<CWrapper**>(data->GetContents()) = this;

  // Mark handle as weak
  Handle<CData> handle(data);
  handle.SetWeakCallback(CWrapper::WeakCallback);
}


CWrapper::~CWrapper() {
  // Do nothing
}


void CWrapper::Ref() {
  if (ref == NULL) {
    ref = new Handle<CData>(Wrap());
  }
  ref_count++;
}


void CWrapper::Unref() {
  ref_count--;
  assert(ref_count >= 0);

  if (ref_count == 0) {
    delete ref;
    ref = NULL;
  }
}


void CWrapper::WeakCallback(CData* data) {
  CWrapper* wrapper = *reinterpret_cast<CWrapper**>(data->GetContents());
  delete wrapper;
}


Value* Arguments::operator[] (const int index) {
  return *(reinterpret_cast<Value**>(this) - index - 1);
}

} // namespace candor
