The continue statement in Amun programming works somewhat like the break statement. Instead of forcing termination, it forces the next iteration of the loop to take place, skipping any code in between.

```
for {
    continue;
}
```

By default continue forces the next iteration of the current loop but you can chooice which loop to focees

```
for 0 .. 10 {
    for 0 .. 10 {
        continue 2;
    }
}
```