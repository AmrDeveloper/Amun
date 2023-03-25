# Jot Programming Language

A Statically typed, compiled general purpose low level programming language built using C++ and LLVM Infrastructure framework, the design inspired from many programming languages such as C/C++, Swift, Go, Kotlin, Rust.

```
import "cstdio"

struct Container <T> {
   value T;
}

fun main() int64 {
    var scon = Container<*char> ("Hello World");
    var icon = Container<int64> (2023);
    printf("%s on %d", scon.value, icon.value);
    return 0;
}
```
### Features
- Static Types
- Type inference
- Functions
- Pointers
- Arrays
- Struct and Packed struct
- Multi Dimensional Arrays
- Strong Enumeration
- Function Pointer
- Local Variables
- Global Variables
- Import and Load statements and blocks
- If, else if and else statements
- If Else Expression
- While statement
- For range, Forever statements
- For Range statement with optional name and step
- Switch Statement and Expression
- Break and continue statements with optional times
- Declare Prefix, Infix, Postfix functions
- Binary, Logical, Comparisons, Bitwise Operators
- Assignments Operatiors =, +=, -=, *=, /=
- Singed and Un Singed Integer types
- Standard C Headers as part of the Standard library
- No implicit casting, every cast must be explicit to be clear
- Defer Statement
- Default initalization value for global and local variables
- Resolving Constants Index, If Expressions at Compile time
- Generic Programming

### Documentations:
  - [Build](https://amrdeveloper.github.io/Jot/build/)
  - [Compile Options](https://amrdeveloper.github.io/Jot/compiler_options/)
  - [Reference](https://amrdeveloper.github.io/Jot/)
  - [Contribution](https://amrdeveloper.github.io/Jot/contribution/)

### License
```
MIT License

Copyright (c) 2022 Amr Hesham

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
