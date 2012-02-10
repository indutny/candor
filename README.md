# Dot lang

```dot
// Keywords: return, scope, new, if, else, while, break

// Primitives
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
+(b, c) {
  return binding_plus(b, c)
}
b + c
+(b, c)
+any_stuff(b, c) {
  return b + c;
}
b +any_stuff c

// Blocks
{
  scope a
}

// Expr blocks
({
  a = 1
})(x)
// is equivalent to
x.a = 1
```

Grammar:

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
callOrFun := member "(" ((expr ",")* expr)? ")" block? |
             member? "(" ((expr ",")* expr)? ")" block
assign := member "=" expr

prefix := ("!" | "--" | "++") expr
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
if := "if" "(" expr ")" block ("else" block)?
while := "while" "(" expr ")" block

statement := (return | break | if | while | block | expr) CR

expr := binop | prefix | postfix |
        blockExpr | assign | callOrFun | member
```
