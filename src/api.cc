#include "candor.h"
#include "heap.h"
#include "heap-inl.h"
#include "code-space.h"

#include <stdint.h> // uint32_t
#include <stdlib.h> // NULL

namespace candor {

using namespace internal;

// Declarations
template Nil* Value::As<Nil>();
template Number* Value::As<Number>();
template Boolean* Value::As<Boolean>();
template String* Value::As<String>();
template Function* Value::As<Function>();
template Object* Value::As<Object>();

template Nil* Value::Cast<Nil>(char* addr);
template Number* Value::Cast<Number>(char* addr);
template Boolean* Value::Cast<Boolean>(char* addr);
template String* Value::Cast<String>(char* addr);
template Function* Value::Cast<Function>(char* addr);
template Object* Value::Cast<Object>(char* addr);

template bool Value::Is<Nil>();
template bool Value::Is<Number>();
template bool Value::Is<Boolean>();
template bool Value::Is<String>();
template bool Value::Is<Function>();
template bool Value::Is<Object>();

Isolate::Isolate() {
  heap = new Heap(2 * 1024 * 1024);
  space = new CodeSpace(heap);
}


Isolate::~Isolate() {
  delete heap;
  delete space;
}


Value* Value::New(char* addr) {
  return reinterpret_cast<Value*>(addr);
}


template <class T>
T* Value::As() {
  return reinterpret_cast<T*>(this);
}


template <class T>
T* Value::Cast(char* addr) {
  return reinterpret_cast<T*>(addr);
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
   default: return false;
  }

  return HValue::GetTag(addr()) == tag;
}


Function* Function::New(Isolate* isolate, const char* source, uint32_t length) {
  char* code = isolate->space->Compile(source, length);
  return Cast<Function>(code);
}


Value* Function::Call(Object* context,
                      uint32_t argc,
                      Value* argv[]) {
  return CodeSpace::Run(addr(), context, argc, argv);
}


Nil* Nil::New() {
  return NULL;
}


Boolean* Boolean::True(Isolate* isolate) {
  return Cast<Boolean>(HBoolean::New(isolate->heap, true));
}


Boolean* Boolean::False(Isolate* isolate) {
  return Cast<Boolean>(HBoolean::New(isolate->heap, false));
}


bool Boolean::IsTrue() {
  return HBoolean::Value(addr());
}


bool Boolean::IsFalse() {
  return !HBoolean::Value(addr());
}


Number* Number::New(Isolate* isolate, double value) {
  return Cast<Number>(HNumber::New(isolate->heap, value));
}


Number* Number::New(Isolate* isolate, int64_t value) {
  return Cast<Number>(HNumber::New(isolate->heap, value));
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


String* String::New(Isolate* isolate, const char* value, uint32_t len) {
  return Cast<String>(HString::New(isolate->heap, value, len));
}


const char* String::Value() {
  return HString::Value(addr());
}


uint32_t String::Length() {
  return HString::Length(addr());
}

} // namespace candor
