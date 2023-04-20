Amun has compile time strong enumeration by default so you can't compare enum element with integer values,

## Declare Enum

```
enum Number {
    ONE
    TWO
}
```

## Declare Enum with Type

Enum element by default has `int32` type but you can declare it with any integers types from int8,

```
enum Number : int8 {
    ONE
    TWO
}
```

You can also give element a value that match the type for example

```
enum Number : int8 {
    ONE = 1,
    TWO = 2,
}
```

## Access Enum element

To access enum element you need to access it using double colon `::` for example

```
var one : Number = Number::ONE;
var two = Number::TWO;
```

## Pass Enum elemnet

To pass enum element as function parameter it must has Enum Name as type and can't pass it as intgers

```
fun pass_number(number Number) void {
    return
}
```

## Enum number of fields

Amun has compile time attribute for enums to get the number of fields for example

```
var c = Number.count;
```