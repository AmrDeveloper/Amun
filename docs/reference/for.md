Currently Amun has theww types of for statement and we may have more in the future

## Forever

Forever statement is a simple infinty for loop inspired from Go design and it recommended
if you want to loop for ever, the advantage over while loop with condition true is that it will 
generate less machine code that don't need to jump to the condition and check if it true then jump
back to the body.

```
for {
    ...
}
```

## For range

For range statement is used to loop over range of values from x to y includes x and y, 
and the variable will be named it by default for example.

```
for 0 to 10 {
    printf("%d", it);
}
```

```
for 'a' .. 'z' {
    printf("%c", it);
}
```

You can explicit change the default variable name from it to any identifier for example.

```
for i : 0 to 10 {
    printf("%d", i);
}
```

```
for c : 'a' .. 'z' {
    printf("%c", c);
}
```

The default loop step is 1 but you can change it for example.

```
for 0 to 10 : 2{
    printf("%d", it);
}
```

## For each

Amun has for each support for single and multi dimensions fixed size arrays, and implicit variables for index and value

```
var array = [1, 2, 3];
for array {
    printf("Index %d, value = %d\n, it_index, it);
}
```

You can set explicit name for each element for example

```
var array = [1, 2, 3];
for element : array {
    printf("Index %d, value = %d\n, it_index, element);
}
```