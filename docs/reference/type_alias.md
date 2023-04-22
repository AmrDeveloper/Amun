# Type alias

Amun has support for type alias to make your code simpler, more readable

It easy to declare type alias with syntax inspired from declaring variables

The grammer for type alias is the following

```
type <name> = <aliased type>;
```

Examples

```
type long = int64;
type iPair = Pair<int64, int64>;
type callback = *() void;
```