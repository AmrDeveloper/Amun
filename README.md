# Jot Programming Language

Staticly typed Low level programming language that use LLVM infrastructure, the design inspired from many programming languages such as C/C++, Swift, Go, Kotlin, Rust.

### Features
- Static Types
- Type inference
- Functions
- Pointers
- Arrays
- Function Pointer
- Variables
- Import and Load block
- If, else if and else statements
- If Else Expression
- While statement
- Declare Prefix, Infix, Postfix functions
- Binary, Logical, Comparisons Operators
- Assignments Operatiors =, +=, -=, *=, /=
- Standard C Headers as part of the Standard library

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
