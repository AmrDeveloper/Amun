Functions in Amun is like function in any other language it take parameter and return value,
but with extra syntax sugger for readability

## Function declaration

```
fun sum(x int64, y int64) int64 {
    return x + y;
}
```

## Single line function declaration

If you have a function that only return expression you can declare it in one line

```
fun duplicate(n int64) int64 = n * 2;
```

## Function declaration with 0 parameter

If your function require 0 parameter you can declare it without `()`

```
fun show_info void {
    ...
    return;
}
```



