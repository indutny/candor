#include "candor.h"
#include "heap.h"
#include "heap-inl.h"
#include "code-space.h"
#include "utils.h"

#include <stdint.h> // uint32_t
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
    V(Object)

#define METHODS_ENUM(V)\
    template V* Value::As<V>();\
    template V* Value::Cast<V>(char* addr);\
    template V* Value::Cast<V>(Value* value);\
    template bool Value::Is<V>();\
    template Handle<V>::Handle(V* v);
TYPES_LIST(METHODS_ENUM)
#undef METHODS_ENUM

#undef TYPES_LIST

static Isolate* current_isolate = NULL;

Isolate::Isolate() {
  heap = new Heap(2 * 1024 * 1024);
  space = new CodeSpace(heap);
  current_isolate = this;
  scope = NULL;
}


Isolate::~Isolate() {
  delete heap;
  delete space;
}


Isolate* Isolate::GetCurrent() {
  // TODO: Support multiple isolates
  return current_isolate;
}


HandleScope::HandleScope() : isolate(Isolate::GetCurrent()) {
  parent = isolate->scope;
  isolate->scope = this;
}


HandleScope::~HandleScope() {
  ValueList::Item* item = references.tail();
  while (item != NULL) {
    isolate->heap->Dereference(
        NULL, reinterpret_cast<HValue*>(item->value()));
    item = item->prev();
  }

  isolate->scope = parent;
}


void HandleScope::Put(Handle<Value> handle) {
  references.Push(*handle);
  isolate->heap->Reference(NULL, reinterpret_cast<HValue*>(*handle));
}


template <class T>
Handle<T>::Handle(T* v) : isolate(Isolate::GetCurrent()), value(v) {
  if (isolate->scope != NULL) {
    isolate->scope->Put(*reinterpret_cast<Handle<Value>*>(this));
  }
}


template <class T>
void Handle<T>::Persist() {
  isolate->heap->Reference(reinterpret_cast<HValue**>(addr()),
                           reinterpret_cast<HValue*>(value));
}


template <class T>
void Handle<T>::Weaken() {
  isolate->heap->Dereference(reinterpret_cast<HValue**>(addr()), NULL);
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
T* Value::Cast(Value* value) {
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
   default: return false;
  }

  return HValue::GetTag(addr()) == tag;
}


Function* Function::New(const char* source, uint32_t length) {
  char* root;
  char* code = Isolate::GetCurrent()->space->Compile(source, length, &root);
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


Value* Function::Call(Object* context,
                      uint32_t argc,
                      Value* argv[]) {
  return Isolate::GetCurrent()->space->Run(addr(), context, argc, argv);
}


Nil* Nil::New() {
  return NULL;
}


Boolean* Boolean::True() {
  return Cast<Boolean>(HBoolean::New(Isolate::GetCurrent()->heap, true));
}


Boolean* Boolean::False() {
  return Cast<Boolean>(HBoolean::New(Isolate::GetCurrent()->heap, false));
}


bool Boolean::IsTrue() {
  return HBoolean::Value(addr());
}


bool Boolean::IsFalse() {
  return !HBoolean::Value(addr());
}


Number* Number::NewDouble(double value) {
  return Cast<Number>(HNumber::New(Isolate::GetCurrent()->heap, value));
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
  return Cast<String>(HString::New(Isolate::GetCurrent()->heap, value, len));
}


const char* String::Value() {
  return HString::Value(addr());
}


uint32_t String::Length() {
  return HString::Length(addr());
}


Value* Arguments::operator[] (const int index) {
  return *(reinterpret_cast<Value**>(this) - index - 1);
}

} // namespace candor
