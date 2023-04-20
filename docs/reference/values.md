Amun has support for primitives types similer to C/C++

## Numbers

Amun has different size of signed and un signed integers and floats

### un signed integers

int8

int16

int32

int64

### signed integers

uint8

uint16

uint32

uint64

### Floats

float32

float64

## BooIean

In Amun you can use `bool` or `int1` to declare boolean type, they are totaly the same

```
var t : bool = true;
var f : int1 = false;
```

## String

Strings are actually one-dimensional array of characters terminated by a null character '\0'

```
var hello : *char = "Hello, World!";
```

## Character

Uses char type to store characters and letters. However, the char type is integer type because underneath C stores integer numbers instead of characters.In C, char values are stored in 1 byte in memory,and value range from -128 to 127.

In order to represent characters, the computer has to map each integer with a corresponding character using a numerical code. The most common numerical code is ASCII, which stands for American Standard Code for Information Interchange.

```
var character : char = 'x';
```

## Null

NULL as a special pointer and its size would be equal to any pointer.

```
var p : *int64 = null;
```