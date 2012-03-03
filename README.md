# DOT

Experimental implementation of DOT language VM.

**Danger! This whole project is not stable at all, many things are broken and
may/will change in future**

Note: only x64 is supported now.

## Language basics

Dot is essentially inspired by the [ECMA-script](http://www.ecmascript.org/),
but has much less features and complexity (for compiler).

Functions are declared in [dart](http://www.dartlang.org/)-like style, variables
are block scoped by default (use `scope` keyword for accessing outer-scope
variables).

```dot
// Keywords: nil, return, scope, new, if, else, while, break

// Primitives
nil
1
'abc'
"abc"
[1, 2, 3]
{ a: 1, 'b': 2, "c": 3 }

// Variables and objects
a = 1
a.b = "abc"
a.b.c = a

// Functions
a() {
  return 1
}
a()

// Blocks
{
  scope a // use this to interact with variables from outer blocks
          // NOTE: the closest one will be choosed
}

// Not implemented yet

// Expr blocks
({
  a = 1
})(x)
// is equivalent to
x.a = 1
```

## Example

```dot
factorial(x) {
  if (x == 1) return 1
  return x * factorial(x - 1)
}

factorial(10)
```

## Building

```bash
git clone git://github.com/indunty/dotlang.git
cd dotlang
make ARCH=x64 test
```

## Status of project

Things that are implemented currently:

* Language lexer and parser
* Assigning on-stack and context variables
* Some binary operations and unary `++` and `--` (both prefix and postfix)
  (No boolean operations so far)
* Unboxing of heap numbers (partially)
* Floating point operations (no conversion to small integers for `%` and `&|^`)
* Function calls, passing arguments and using returned value
* Stop-the-world copying garbage collector
* Hash-maps (objects), numeric and string keys

Things to come:

* Finishing binary and unary operations
* String concatenation
* On-stack replacement and profile-based optimizations
* Incremental GC
* Usage in multiple-threads (aka isolates)
* Calling C++ functions from dotlang
* See [TODO](https://github.com/indutny/dotlang/blob/master/TODO) for more
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

blockExpr := block

return := "return" expr?
break := "break"
if := "if" "(" expr ")" block ("else" block)? |
      "if" "(" expr ")" stmt
while := "while" "(" expr ")" block

statement := (return | break | if | while | block | expr) CR

expr := binop | prefix | postfix |
        blockExpr | assign | callOrFun | member
```

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
