# Candor C++ API

Candor is meant to be an embedded VM to add scripting capabilities to your
application.  This document explains the candor C++ embedding API.

When in doubt, check `candor.h` in the include directory.  That is the
definitive source of information.

## candor::Isolate

You need to have an Isolate instance in your thread so that candor has a world
to store everything in it's vm.  The easiest way to create one is to have an
instance in your main function:

```C++
// Create a new Isolate
Isolate isolate;
```

This can also be used to get at syntax errors in the compiler.

```C++
Isolate* isolate = Isolate::GetCurrent();
if (isolate->HasError()) {
  // You can get the error
  Error* error = isolate->GetError();
  // and do something with it. 

  // Or just print it.
  isolate->PrintError();
}
```

## candor::Value

This is the base class for all candor values.  It contains the types `Nil`,
`Number`, `Boolean`, `String`, `Function`, `Object`, `Array`, and `CData`.

Use this type if you want to accept any type as input or return any type as
output.  Candor is a loosly typed language, so you'll be seeing a log of this
class.

### Handles to values

The Handle type is not a Value subclass.  It's a utility to give handles to
Value* instances.  Since candor is a scripting language with a dynamic GC, the
actual Value* pointers can be moved around by the GC.  It's only safe to use
Value* pointers within a single function scope.  If you're wanting to use those
values later (like in an async callback), you must use a handle to a value
instead.

Also handles mark a value as being a new GC root.  This prevents them from being
collected.  This is required if your value isn't accessable from some already
rooted value and you intend for it to stick around.

A Handle's hold on a value lasts for the lifetime of the handle instance itself.
Usually this will be a static instance, but it can be managed using `new` and
`delete` as well.

There are two ways to point a handle to a value.  One is to use the Handle
constructor with a value as input, the other is to call `Handle::Wrap(value)`
after the fact.

Handle overloads the `*` and `->` operators to return the value so that it acts
mostly like the value it's pointing to.

```C++
// Create a new object and wrap it in a handle using the constructor.
Handle<Object> global(Object::New());

// Create an empty handle and wrap it around a value later.
Handle<Function> callback;
callback.Wrap(Function::New(myCallbackFn));

// I'm done wrapping the value and want to let it go
callback.Unwrap();
```

If you want to know if a handle is pointing to anything, use
`Handle::IsEmpty()`.

```C++
// Use a handle to do a memoizing function
static Handle<Object> module;
Object* loadModule() {
  if (!module.IsEmpty()) {
    return *module
  }
  module.Wrap(Object::new());
  
  // initialize module with something interesting

  return *module;
}
```

### Value type checking

Sometimes you want to know for sure if a value is of a certain type.  This is
done with the `Value::Is<T>` method template.

```C++
// value is a Value*
const char* myToString(Value* value) {
  if (value->Is<Nil>()) { return "nil"; }
  if (value->Is<Object>()) { return "[Object]"; } 
  if (value->Is<Array>()) { return "[Array]"; } 
  if (value->Is<Function>()) { return "[Function]"; } 
  if (value->Is<CData>()) { return "[CData]"; } 
  // ...
}
```

You can also get the type with the `Value::Type()` method.

```C++
switch (value->Type()) {
  case Value::kString: {
    String* string = value->ToString();
    printf("\"%.*s\"", string->Length(), string->Value());
    break;
  }
  case Value::kNumber: 
  case Value::kBoolean: {
    String* string = value->ToString();
    printf("%.*s", string->Length(), string->Value());
    break;
  }
  case Value::kFunction:
    printf("function: %p", value);
    break;  
  case Value::kObject:
    printf("object: %p", value);
    break;
  case Value::kArray:
    printf("array: %p", value);
    break;
  case Value::kCData:
    printf("cdata: %p", value);
    break;
  case Value::kNil:
    printf("nil");
    break;
  case Value::kNone:
    printf("none");
    break;
}
```

### Value type coercion

You can coerce any type into a `Number`, `Boolean` or `String`.  This is a
lossly operation and can result in `0`, `false`, or `""` if there is no
sensible way to convert the input.

```C++
// value is a Value*
Number* asNumber = value->ToNumber();
Boolean* asBoolean = value->ToBoolean();
String* asString = value->ToString();
```

### Value conversion

In addition to type coercion, you can do type conversion with the
`Value::As<T>` method.  This internally does an assert on `Is<T>()`, so be
sure to not use it with the wrong type or the assert will fail.

```C++
// value is a Value*, but we hope it's a function too.
if (value->Is<Function>()) {
  Function* fn = value->As<Function>();
  Value* result = fn->Call(0, NULL);
})
```

### Weak Callback

If you want to be notified when a value is about to be GC'ed then you can use
`Value::SetWeakCallback()` and `Value::ClearWeak()`.

```C++
static void onWeak(Value* value) {
  // Do something, maybe cleanup.
}
// value is Value*
value->SetWeakCallback(onWeak);
// Then later you decided you don't want to be notified anymore
value->ClearWeak();
```

## candor::Nil

The Nil class is simple, it creates Nil values.

```C++
Value* someFunc(uint32_t argc, Arguments& argv) {
  // Do some work and then return Nil
  return Nil::New();
}
```

## candor::Boolean

This is almost as simple as Nil, except it can hold binary state.  There are
three contructors for this class.

```C++
Boolean* one = Boolean::New(someExpression);
Boolean* two = Boolean::True();
Boolean* three = Boolean::False();

if (one->IsTrue()) {
  // Do something if someExpression was truthy
}
```

## candor::Number

Numbers can hold integers and floats.  There are different constructors and
access methods for each.

```C++
Number* foo = Number::New(3.14159265358979);
Number* bar = Number::NewIntegral(42);
```

Also, you can use `Number::IsIntegral()` to see if the contents are integral.

```C++
if (bar->IsIntegral()) {
  int64_t num = bar->IntegralValue();
  // do something with this
} else {
  double num = bar->Value();
  // do something with this
}
```

## candor::String

String values have two constructors, one for null terminated strings and
another for strings with an explicit length.

```C++
// Initialize with a null terminated const char*
String name = String::New("Tim");
// Initialize with a char* and length
String buffer = String::New(buf.base, buf.length);
```

You can get the data back out of a string using `String::Length()` and
`String::Value()`.  Be careful, the `const char*` returned by `Value()` is not
null terminated, so make sure to use `Length()` to get the size.

```C++
// Print a String* value str
printf("%.*s", str->Length(), str->Value());
```

## candor::Function

This class is used to represent function values.  In candor, functions are
first-class values meaning they can be used as arguments as well as return
values or just stored in variables or objects properties.  There are two kinds
of functions, those created in a script and those that wrap native C++
functions.

### Function from script.

To create a function from a script, simply pass in the source code to the
constructor.

```C++
Function* fn = Function::New("return 1 + 2");
// buf is a struct with .base and .len properties (not null terminated)
Function* fn2 = Function::new(buf.base, buf.len);
```

### Function from C++ function

To create a native function, simply pass the function pointer
(BindingCallback*) into the constructor.

```C++
// Define a BindingCallback function
Value* myPrint(uint32_t argc, Arguments& argv) {
  assert(argc == 1);
  String* str = argv[0]->ToString();
  printf("%.*s\n", str->Length(), str->Value());
  return Nil::New();
}
```

Then later create a candor function value that wraps this C++ function.

```C++
Function* fn = Function::New(myPrint);
```

### Setting a function's context

A function can have a global context set so that you're able to inject
variables into the environment.

```C++
Function* main = new Function(mainScript, strlen(mainScript));
Object* global = Object::New();
global->Set("print", Function::New(myPrint));
main->SetContext(global);
```

Also you can retreive the context from a function

```C++
Object* context = fn->GetContext();
```

### Calling a Function from C++

Calling functions is very easy in candor using `Function::Call()`.  Simply
provide an arguments array and it returns a `Value*` return value.

```C++
// addFn is a Function* that accepts two numbers and adds them.
// Set up the arguments
Value* argv[2];
argv[0] = Number::NewIntegral(3);
argv[1] = Number::NewIntegral(5);
// Make the call
Value* result = addFn->Call(2, argv);
// Print the result
printf("The result is %d\n", result->ToNumber()->IntegralValue());
```

## candor::Array

Arrays are containers that contain many values.  The keys can be any positive
integers.  Also there is a `Array::Length()` method that gives the largest key
+ 1 (which is the array length if it's dense).

You can get, set, and delete values from an Array instance.

```C++
// Store argv in an args Array
Array* args = Array::New();
for (int i = 0; i < argc; i++) {
  args->Set(i, String::New(argv[i]));
}

// Read the second item
Value* path = args->Get(1);

// Delete the third item
args->Delete(2);

// Print the length
printf("args length: %d\n", args->Length());
```

## candor::Object

Objects in candor can hold arbitrary Values as keys and values.  This is a
very powerful and flexible data structure.

### Creating an object.

There are two ways to create an object.  Either create a new empty object or
clone an existing object.  The clone is a shallow copy of all the properties.
Once cloned, there is no relationship between the parent and child objects.

```C++
Object* parent = Object::New();
Object* clone = parent->Clone();
```

### Getting, Setting, and Deleting properties

The C++ API lets you read, write, and delete properties in Objects.  Since
keys can be any arbitrary value type, but are most often strings, there are
two versions of all these methods for convenience.

```C++
Object* parent Object::New();
// Set some properties.
parent->Set("name", String::New("Tim"));
parent->Set("age", Number::NewIntegral(29));

// Store a password in an unguessable key
parent->Set(Object::New(), String::New("C@nd0rR0cks!"));

// Maybe we don't want to store the age in there.
parent->Delete("age");
```

To loop over the keys and values in this object, we can use the
`Object::Keys()` method which returns an Array of all the keys. (including our
anonymous Object above).

```C++
// Lets create a new object that swaps keys and values.
Object* obj = Object::New();
Array* keys = parent->Keys();
int64_t i = keys->Length();
while (i--) {
  Value* key = keys->Get(i);
  Value* value = obj->Get(key);
  obj->Set(value, key);
}
```

## candor::CData

The CData type is much like the String type, except it's meant for holding
arbitrary void* data.  The most common use case for this is to pass C struct
or C++ class instances to candor script.

The constructor accepts a size parameter and does the memory allocation for
you.  The memory is then managed by the VM.  Use `Value::SetWeakCallback()` if
you need to do some cleanup when the value gets GCed.

```C++
// Allocate the memory inside the VM as a CData value.
CData* cdata = CData::New(sizeof(uv_timer_t));
// Pull the memory out and cast to a C struct instance.
uv_timer_t* timer = (uv_timer_t*)cdata->GetContents();
//...
```

## candor::CWrapper

CWrapper is a base C++ class that's meant to be inherited from.  It makes it
easy to use CData with C++ classes.

Here is a sample class that uses the CWrapper convencience wrapper around CData:

```C++
class MyBox : public candor::CWrapper {
  int width;
  int height;
 public:
  MyObject(int w, int h) {
    width = w;
    height = h;
  }

  int Area() {
    return width * height;
  }

  ~MyObject() {
    // This is called when the CData is about to be garbage collected.
  }
  
}
```

To create a new instance of it and get the cdata we do:

```C++
MyBox* box = new MyBox(3, 4);
CData* cdata = box->Wrap();
```

Then later when the cdata is passed to us in a function, we unwrap it.

```C++
// Expects the cdata as the first argument, returns the area of the box.
Value* box_area(uint32_t argc, Arguments& argv) {
  assert(argc == 1);
  CData* cdata = argv[0]->As<CData>();
  MyBox* box = CWrapper::Unwrap<MyBox>(cdata);
  int area = box->Area();
  return Number::NewIntegral(area);
}
```
