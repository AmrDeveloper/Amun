Amun gives you the same level of freedom of dealing with memory managment like C/C++,
there is no garbage collection at all, 

## Pointer type

To create a pointer type you need to write the type prefix with `*` for example

```
var x : *int64;
var y : *float64;
```

## Derefernce

To derefernce pointer you need to use derefernce operator `&` similer to C

```
var p : *int64 = create_value();
var v : int64 = &p;
```

## Create and Free memory

Currently the only way to create and free memory is to use `libc` functions such as `malloc` and `free`