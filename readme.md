# The Ape Programming Language

## Try Ape in your browser on [Ape Playground](https://kgabis.github.io/apeplay/).

## About
Ape is an easy to use programming language and library written in C. It's an offspring of [Monkey](https://monkeylang.org) language (from [Writing An Interpreter In Go](https://interpreterbook.com) and [Writing A Compiler In Go](https://compilerbook.com) books by [Thorsten Ball](https://thorstenball.com)), but it evolved to be more procedural with variables, loops, operator overloading, modules, and more.

### This fork in particular
This fork focuses on making Ape slightly more Javascript-ish, without *being* Javascript. Think of it as a Lite variant of Ecmascript.

## Current state
It's under development so everything in the language and the api might change.

## Example
```javascript
function contains_item(to_find, items) {
    for (item in items) {
        if (item == to_find) {
            return true
        }
    }
    return false
}

const cities = ["Warszawa", "Rabka", "Szczecin"]
const city = "Warszawa"
if (contains_item(city, cities)) {
    println(`found ${city}!`)
}
```

## Embedding
```c
#include "ape.h"

int main()
{
    ApeContext_t* ctx;
    ctx = ape_make_context();
    /* last option is the 'data' pointer passed to the function! */
    ape_context_setnativefunction(ctx, "myfunc", myfuncptr, NULL);
    ape_context_executesource(ctx, "println(\"hello world\")");
    ape_context_destroy(ctx);
    return 0;
}
```

The best way to get an idea of how to embed Ape in C is currently `main.c` - though, the above code should make it fairly clear.

## Language

Ape is a dynamically typed language with mark and sweep garbage collection. It's compiled to bytecode and executed on internal VM. It's fairly fast for simple numeric operations and not very heavy on allocations (custom allocators can be configured). More documentation can be found [here](documentation.md).

### Basic types
```bool```, ```string```, ```number``` (double precision float), ```array```, ```map```, ```function```, ```error```

### Operators
```
Math:
+ - * / %

Binary:
^ | & << >>

Logical:
! < > <= >= == != && ||

Assignment:
= += -= *= /= %= ^= |= &= <<= >>=
```

### Defining constants and variables
```javascript
const constant = 2
constant = 1 // fail
var variable = 3
variable = 7 // ok
```

## Strings
```javascript
const str1 = "a string"
const str2 = 'also a string'
const str3 = `a template string, it can contain expressions: ${2 + 2}, ${str1}`
```

### Arrays
```javascript
const arr = [1, 2, 3]
arr[0] // -> 1
```

### Maps
```javascript
const map = {"lorem": 1, 'ipsum': 2, dolor: 3}
map.lorem // -> 1, dot is a syntactic sugar for [""]
map["ipsum"] // -> 2
map['dolor'] // -> 3
```

### Conditional statements
```javascript
if (a) {
    // a
} else if (b) {
    // b
} else {
    // c
}
```

### Loops
```javascript
while (true) {
    // body
}

var items = [1, 2, 3]
for (item in items) {
    if (item == 2) {
        break
    } else {
        continue
    }
}

for (var i = 0; i < 10; i++) {
    // body
}
```

### Functions
```javascript
const add_1 = function(a, b) { return a + b }

function add_2(a, b) {
    return a + b
}

function map_items(items, map_fn) {
    const res = []
    for (item in items) {
        append(res, map_fn(item))
    }
    return res
}

map_items([1, 2, 3], function(x){ return x + 1 })

function make_person(name) {
    return {
        name: name,
        greet: function() {
            println(`Hello, I'm ${this.name}`)
        },
    }
}
```

### Errors
```javascript
const err = error("something bad happened")
if (is_error(err)) {
    println(err)
}

function() {
    recover (e) { // e is a runtime error wrapped in error
        return null
    }
    crash("something bad happened") // crashes are recovered with "recover" statement
}
```

### Modules
```javascript
import "foo" // import "foo.ape" and load global symbols prefixed with foo::

foo::bar()

import "bar/baz" // import "bar/baz.ape" and load global symbols prefixed with baz::
baz::foo()
```

### Operator overloading
```javascript
function vec2(x, y) {
    return {
        x: x,
        y: y,
        __operator_add__: function(a, b) { return vec2(a.x + b.x, a.y + b.y)},
        __operator_sub__: function(a, b) { return vec2(a.x - b.x, a.y - b.y)},
        __operator_minus__: function(a) { return vec2(-a.x, -a.y) },
        __operator_mul__: function(a, b) {
            if (is_number(a)) {
                return vec2(b.x * a, b.y * a)
            } else if (is_number(b)) {
                return vec2(a.x * b, a.y * b)
            } else {
                return vec2(a.x * b.x, a.y * b.y)
            }
        },
    }
}
```

To see more code, just read the `*.ape` files scattered about!

## Build, etc

There's no fancy build setup in place right now, but `make` *should* build cleanly.  
You may still need `cproto` in your $PATH if you modify sources (specifically, function prototypes); 
in that case, `apt install cproto` will set you up on debian/ubuntu/wsl. Cygwin also has cproto.  
The makefile contains some kludge to figure out if you have cproto; needs `make` impl that isn't ancient (~>2.\* should be fine).

## License
[The MIT License (MIT)](http://opensource.org/licenses/mit-license.php)
