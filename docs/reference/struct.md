## Structure

Amun has structures type similer to C and Go

## Declaration

```
struct Point {
    x int64;
    y int64;
}
```
## Packed Struct Declaration

To declare struct with no padding between fields you need to use `#packed` directive

```
// 1 (with 3 padding) + 4 + 1 (with 3 padding) = 12 bytes
struct UnPackedStruct {
    a int8;
    b int32;
    c int8;
}

// 1 + 4 + 1 = 6 bytes
@packed struct PackedStruct {
    a int8;
    b int32;
    c int8;
}
```

## Initialization

No need for `typedef` similer to C to declare variable without keyword struct, just initialize it

```
var point : Point;
```

## Constructor

You can initialize structure and assign values for all fields using initialize expression for example

```
struct Vector3 {
    x int64;
    y int64;
    z int64;
}

var vector3 = Vector3(1, 2, 3);
```

## Destructor

Current Amun design has not destructor, but for destructors you can easily depend on `defer` feature to got the same feature for example

```
var point : *Point = create_poinnt();
defer delete_point(point);
```

## Access fields

To access field from struct that initialized on the stack or the heap in both you will use dot `.` to access it

```
var x = point.x
```

## Modify field value

To modify struct field value you can also use dot `.` to update it

```
point.x = 10;
```