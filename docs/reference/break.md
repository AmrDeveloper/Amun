The break statement ends the loop immediately when it is encountered

```
for {
    break;
}
```

By default breakends the current loop immediately but you can chooice which loop to end

```
for 0 .. 10 {
    for 0 .. 10 {
        break 2;
    }
}
```