## Operator overloading

Amun supports operator overloading, some operators can be used to accomplish different tasks based on their input arguments, This is possible because operators are syntactic sugar for method calls, 
for example `+` opertor in a + b will converted to `add(a, b)` for those types


Operators that can be overloaded include `+`, `-`, `+`, `/`, `%`, `==`, `!=`, `>=`, `<=`, `>`, `<`, `<<`, `>>`, `&`, `|`, `&&`, `||`.

Note: Any overloaded operator function must has at least non primitives type

```
struct Pair {
    x int64;
    y int64;
}

operator + (f Pair, s Pair) Pair {
    return Pair(f.x + s.x, f.y + s.y);
}

fun main() {
    var p1 = Pair(1, 2);
    var p2 = Pair(3, 4);
    var p3 = p1 + p2;
}
```