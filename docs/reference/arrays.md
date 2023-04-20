Amun has single and multi dimensions arrays

## Fixed size Array type

To create a fixed size array type, you need to prefix the type with size `[Size]T` for example

```
[10]int64
[15]float64
```

## Declare one dimention array

To create array of values you need to write values spreated by comma `,` between `[` and `]`

```
var array = [1, 2, 3];
var [3]array = [1, 2, 3];
```

## Declare Multi dimentions array

Multi dimensions array is an array of arrays so it just arrays spreated by comma inside one array

```
var arrays = [[1, 2, 3], [4, 5, 6], [7, 8, 9]];
var [3][3]arrays = [[1, 2, 3], [4, 5, 6], [7, 8, 9]];
```

## Access Array element by index

To access array element by index you need to use index expression for example

```
array[0]
array[0][0]
array[0][0][0]
```

## Modify Array element by index


To modify array element by index you need to use index as lvalue for example

```
array[0] = value;
array[0][0] = value;
array[0][0][0] = value;
```
