In Amun defer statement is used to make call statement to called as before ending the current scope

To make and call expression as defered you need to prefix it with defer keyword for example

```
defer printf("World");
printf("Hello");
```

The output for this snippet will be `HelloWorld`.

Defer statement is very useful feature that can be used to free resource or close strams for example

```
var point = create_point();
defer delete_point(point);
print_point(point);
```

You can see how it useful in case that you allocated memoey in function that has many return statements inside 
nested scopes, so in the normal case you need to free this memory before each return statement

For example without defer statement

```
fun function() void {
    var memory = allocate_memory();

    if condition {
        free_memory(memory);
        return;
    }

    if condition2 {
        free_memory(memory);
        return;
    }

    free_memory(memory);
    return;
}
```

The same example with defer statement will be

```
fun function() void {
    var memory = allocate_memory();
    defer free_memory(memory);
    
    if condition return;
    if condition2 return;
    
    return;
}
```