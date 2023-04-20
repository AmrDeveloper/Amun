Amun has both If statement and Expression

## If Statement

If Statement is the same line in C/C++ it execute the block only if the condition is not evaluated to false

```
if condition {

}
else if condition2 {

}
else {

}
```

## If Else Expression

If Else Expression is used to return one of two values depend on the condition for example

```
var result = if (condition) value1 else value2;
```

## Compile time If Else expression

If the condition and values of if else are constants and can resolved during the compile time you can use it as a value for global variable

```
var is_debug = true;
var build_info = if (is_debug) 10 else 20;

fun main() int64 {
    return 0;
}
```