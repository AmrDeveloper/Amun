<p align="center">
<img src="assets/logo.svg" width="20%" height="20%"/>
</p>

A Statically typed, compiled general purpose low level programming language built using C++ and LLVM Infrastructure framework, the design was inspired from many programming languages with the goal to be simple and productive

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
- Tuples
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

## Inspiration
The design of Jot is inspired by a number of languages such as `C`, `C++`, `Go`, `Rust`, `Jai`,
special thanks for every create language designer and for every open source project that share
the creativity and knowledge.
