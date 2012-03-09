# Candor

Candor is a language inspired by javascript, but with less features and,
therefore, less complexity. So no semicolons, no exceptions and simplified
anonymous function syntax (dart-like).

Main goal of Candor is to provide a powerful and developer-friendly language
that can be easily optimized by compiler.

## Description

Experimental implementation of Candor language VM. Join #candor channel on
irc.freenode.net to discuss language features.

**Danger! This whole project is not stable at all, many things are broken and
may/will change in future**

Note: only x64 is supported now.

## Language basics

Candor is essentially inspired by the [ECMA-script](http://www.ecmascript.org/),
but has much less features and complexity (for compiler).

Functions are declared in [dart](http://www.dartlang.org/)-like style, variables
are block scoped by default (use `scope` keyword for accessing outer-scope
variables).

```candor
// Keywords: nil, true, false, typeof, sizeof, keysof, scope, if, else, while,
// for, break, continue, return, new

// Primitives
nil
true
false
NaN
1
'abc'
"abc"
[1, 2, 3]
{ a: 1, 'b': 2, "c": 3 }

// Variables and objects
a = 1
a.b = "abc"
a.b.c = a
a[b()][c] = x

// While object literals are restricted to declaring strings as keys, any value
// can be used as a key. This allows for all kinds of interesting data
// structures like efficient sets and unique unguessable keys.
a = { "5": "five" }
a[5] = 5
a["5"]         // -> "five"
a[5]           // -> 5
a[{ hello: "World" }] = "key is object, value is string!"

// Functions
a() {
  return 1
}
a()
// Functions are also objects and can have properties
a.b = "foo"

// Arrays are also objects, except they internally keep track of the largest
// integer index so that sizeof works with them.
a = [1,2,3]
a.foo = true
sizeof a       // -> 3
a.foo          // -> true

// typeof.  Sometimes it's useful to know what type a variable is

typeof nil     // -> "nil"
typeof true    // -> "boolean"
typeof false   // -> "boolean"
typeof 42      // -> "number"
typeof "Hello" // -> "string"
typeof [1,2,3] // -> "array"
typeof {a: 5}  // -> "object"
typeof (){}    // -> "function"

// sizeof gives the size of an array (max integer key + 1) or string (number of bytes)
// gives nil for other types

sizeof "Hello" // -> 5
sizeof [1,2,3] // -> 3
sizeof {}      // -> 0

// keysof returns an array of all the keys in an object
keys = keysof { name: "Tim", age: 29 }
keys           // -> ["name", "age"]

// Blocks
{
  // Blocks allow you to combine multiple statements into a single
  // statement.  For example, the body of an `if or `while` loop. Blocks also
  // create a new local variable scope.

  scope a // use this to interact with variables from outer blocks
          // NOTE: the closest one will be chosen
}

// Control flow

// The variables in the condition head are scoped with the condition, not the
// optional body block.

// Conditionals
person = { age: 29, name: "Tim" }

// With block
if (person.age > 18) {
  scope person
  person.name  // -> "Tim"
}

// Without block
if (person.age > 18) person.name

// using else
if (person.age > 18) {
  scope person
  // do something with `person`
} else {
  // do something else
}

if (person.age > 18) action(person)
else otherAction()

// While loops
i = 0
sum = 0
while (i < 10) {
  scope i, sum
  sum = sum + i
  i++
}

// break and continue. `while` loop can have `break` and `continue`
// break exits a loop immediately, continue, skips to the next iteration

```

## Example

```candor
// Defining a recursive function
factorial(x) {
  if (x == 1) return 1
  return x * factorial(x - 1)
}

factorial(10)

// Implementing a forEach function
forEach(array, callback) {
  if (typeof array != "array") return
  length = sizeof array
  i = 0
  while (i < length) {
    callback(i, array[i])
    i++
  }
}

// Implementing switch with chained if..else
type = typeof value
if      (type == "nil")     handleNil(value)
else if (type == "boolean") handleBoolean(value)
else if (type == "number")  handleNumber(value)
else if (type == "string")  handleString(value)
else handleObject(value)

// Implementing switch using objects
handlers = {
  "nil":     handleNil,
  "boolean": handleBoolean,
  "number":  handleNumber,
  "string":  handleString
}
handler = handlers[typeof value]
if (handler) handler(value)
else handleObject(value)

```

As you can see Candor's syntax is very close to the ecmascript's one. But
there're no semicolons, statements are separated by newline symbols (whitespace
is ignored).

## Building

```bash
git clone git://github.com/indutny/candor.git
cd candor
make ARCH=x64 test
```

## Status of project

Things that are implemented currently:

* Language lexer and parser
* Assigning on-stack and context variables
* Binary and unary operations
* Unboxing of heap numbers
* Floating point operations
* Function calls, passing arguments and using returned value
* Stop-the-world copying two-space garbage collector
* Hash-maps (objects), numeric and string keys
* Arrays
* Typeof, Sizeof, Keysof
* String concatenation
* C++/C bindings support for candor

Things to come:

* C++/C bindings documentation
* On-stack replacement and profile-based optimizations
* Incremental GC
* Usage in multiple-threads (aka isolates)
* Inline caching
* Break statement
* Better parser/lexer errors
* See [TODO](https://github.com/indutny/candor/blob/master/TODO) for more
  up-to-date tasks

## Contributing

Any bug-fixes or feature implementations are always welcome! Only one
restriction is applied to the code - it should follow
[Google C++ Style Guide](http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml).

## Grammar (basics)

```
code := statement*

scope := "scope" (name ",")* name CR
block := "{" scope? statement* "}"
args := ((name ",")* name)?

array := "[" ((expr ",")* expr)? "]"
objectKv := (name|string) ":" expr
object := "{" ((objectKv ",")* objectKv)? "}"

primary := name | number | string | array | object | "(" expr ")"
member := primary ("[" expression "]" | "." name)*
callOrFun := member ("(" ((expr ",")* expr)? ")")+ |
             member "(" ((expr ",")* expr)? ")" block |
             member? "(" ((expr ",")* expr)? ")" block
assign := member "=" expr

prefix := ("!" | "--" | "++", "-", "+") expr
postfix := expr ("--" | "++")

binop1 := expr (("*" | "/") expr)*
binop2 := binop1 (("+" | "-") binop1)*
binop3 := binop2 (("&" | "|" | "^") binop2)*
binop4 := binop3 (("&&" | "||") binop3)*
binop5 := binop4 ((">" | "<" | "<=" | ">=") binop4)*
binop := binop5 (("==" | "===" | "!=" | "!==") binop5)*

return := "return" expr?
break := "break"
if := "if" "(" expr ")" block ("else" block)? |
      "if" "(" expr ")" stmt
while := "while" "(" expr ")" block

statement := (return | break | if | while | block | expr) CR

expr := binop | prefix | postfix |
        assign | callOrFun | member
```

## Credits

Special thanks to [creationix](https://github.com/creationix) for suggesting the
name of this project!

#### LICENSE

Copyright (c) 2012, Fedor Indutny.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
