# Jot Programming Language

A Statically typed, compiled general purpose low level programming language built using C++ and LLVM Infrastructure framework, the design inspired from many programming languages such as C/C++, Swift, Go, Kotlin, Rust.

### Features
- Static Types
- Type inference
- Functions
- Pointers
- Arrays
- Multi Dimensional Arrays
- Enumeration
- Function Pointer
- Local Variables
- Global Variables
- Import and Load statements and blocks
- If, else if and else statements
- If Else Expression
- While statement
- Declare Prefix, Infix, Postfix functions
- Binary, Logical, Comparisons, Bitwise Operators
- Assignments Operatiors =, +=, -=, *=, /=
- Standard C Headers as part of the Standard library
- No implicit casting, every cast must be explicit to be clear
- Defer Statement

### Requirements for Developments
- C++ Compiler (GCC or Clang) that support C++20
- LLVM 14
- Cmake
- Python
- Clang Format
- Ninja (Optional)
- CCashe (Optional)

### Download and Build

```
git clone https://github.com/AmrDeveloper/Jot
cd Jot
mkdir build
cd build
cmake ..
make
```

### Format the C/C++ Code

```
python scripts/format_code.py
```

### Check the front end for all samples

```
python scripts/check_samples.py
```

### Check that all samples are compiled without error on the LLVM Backend

```
python scripts/compile_samples.py
```

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
