Amun has both switch statement and expression

## Switch Statement

Switch statement is the same like in C/C++ it execute block if his case value
match the condition value, if no value match it will execute an optional default block

```
switch (10) {
    1       -> printf("One");
    2, 4    -> printf("Two or four");
    else    -> printf("Else");
}
```

## Switch Expression

Switch expression is used to return a value of branch if his case match the condition value, if no value match it will return the value of default block

```
var result = switch (10) {
    2, 4, 6, 8 -> true;
    else -> false;
};
```

## Compile time switch expression

If the condition and all values are constants and can resolved during the compile time you can use it as value for global variable

```
var build_flavor = Build::RELEASE;
var config = switch (build_flavor) {
    Build::DEBUG   -> 1;
    Build::RELEASE -> 2;
    else           -> 3;
};

fun main() int64 {
    return 0;
}
```