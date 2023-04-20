Amun has support for lambda expression with explicit and implicit capture

### Lambda Expression

Lambda expression start with `{` and end with `}` similer to block statement but with extra `() T ->` to declare the parameters and return types

for example to create lambda expression that take two integers and return the sum you can write it like this

```
var sum_lambda = { (x int64, y int64) ->
    return x + y;
};

var result = sum_lambda(1, 1);
```

If your lambda has no explicit parameters and return void Amun has syntax sugger for this case for example

```
var hello = {
    printf("Hello, World!");
};

hello();
```

You can also call the lambda directly for example

```
var one = { () int64 -> return 1; }();
```

### lambda outside parentheses

In Amun if your function call last parameter type is lambda you can write it outside the parentheses for example

```
fun do(callback *() void) void {
    callback();
}

fun main() int64 {
    do() {
        printf("Hello, World!");
    }
    return 0;
}
```