## Infix function

Amun has support for `infix` keyword inspired from `switt` and `kotlin` design, simpily it used to mark
function with two parameter to be used as nomral function and also as infix operator

```
@infix fun plus(x int64, y int64) int64 = x + y;

fun main() int64 {
    var result1 = plus(10, 20);
    var result2 = 10 plus 20;
    return 0;
}
```