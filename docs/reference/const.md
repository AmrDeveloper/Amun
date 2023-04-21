Compile time constants declaraions

Sometimes a certain value is used many times throughout a program, and it can become inconvenient to copy it over and over. What’s more, it’s not always possible or desirable to make it a variable that gets carried around to each function that needs it. In these cases, the const keyword provides a convenient alternative to code duplication:

Global Compiler time constants declarations

```
const ONE = 1;
const TWO = 2;

const NEW_LINE = '\n';
const IS_DEBUG = true;
```

You can also make compile time constants declaraions local only to functions scope

```
fun function() {
    const ONE = 1;
    const TWO = 2;
}
```