## Prefix function

Amun has support for `prefix` keyword inspired from `switt` design, simpily it used to mark
function with one parameter to be used as nomral function and also as postfix operator

```
@prefix fun not(b bool) bool = !b;

fun main() int64 {
    var result = not true;
    return 0;
}
```