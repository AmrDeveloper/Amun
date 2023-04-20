Amun has no implict casting even between integers types, every cast must be explicit

To cast from your expression to another type for example to int64 you need to prefix the value with cast expression for example

```
var integer = cast(int64) float_value;
```

If the cast is impossitable for example casting between two un castable types you will get a clear compile
time error