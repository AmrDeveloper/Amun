Generics Programming

Amun has supporting for generic programming and give you the ability to create generic functions and structures

Here is an example on how to To create declare generic parameter for struct

```
struct Pair <T> {
    first T;
    second T;
}
```

When you use this type you need spicify the generic parameter you declared them before

for example to create a Pair of int64 you can write it like this

```
var ipairs : Pair<int64>;
```

You can create a pair of other pairs of of pairs pointers

```
var pair_of_pairs : Pair<Pair<int64>>;
var pair_of_pairs_ptr : Pair<*Pair<int64>>;
```

You can also create a pair of array of any type for example

```
var pair_of_array : Pair<[10]int64>;
```

Declaring a generic function is not very different from generic struct for example

```
fun sum<T> (x T, y T) T {
    return x + y;
}
```

And when calling generic function you neet also to spicify the generic parameter for example

```
sum<int64>(1, 1);
sum<intfloat>(1.0, 1.0);
```