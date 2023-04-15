## Tuples

A tuple is a collection of values of different types. Tuples are constructed using parentheses `()`, and each tuple itself is a value with type signature `(T1, T2, ...)`, where `T1`, `T2` are the types of its members. Functions can use tuples to return multiple values, as tuples can hold any number of values.

```
var ituples = (1, 2, 3, 4);
```

```
fun print_tuple(tuple (int64, int64)) {
    printf("%d\n", tuple.0);
    printf("%d\n", tuple.1);
}
```

```
struct Tuples {
    ituple (int64, int64);
    ftuple (float64, float64);
}
```

```
fun return_ituple(x int64, y int64) (int64, int64) {
    return (x, y);
}
```

```
fun make_tuple<T>(x T, y T) (T, T) {
    return (x, y);
}
```

```
var tuple = ([1, 2, 3], ["a", "b", "c"]);
for 0 .. 2 {
    printf("%d - %s\n", tuple.0[it], tuple.1[it]);
}
```