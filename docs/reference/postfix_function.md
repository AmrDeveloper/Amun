## Postfix function

Amun has support for `postfix` keyword inspired from `switt` design, simpily it used to mark
function with one parameter to be used as nomral function and also as postfix operator

```
@postfix fun positiveOrZero(x int64) int64 {
    return if (x > 0) x else 0;
}

fun main() int64 {
    var result = -5 positiveOrZero;
    return 0;
}
```