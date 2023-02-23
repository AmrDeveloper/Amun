Currently the Compiler CLI options is your best friend to interact with the compiler (this may changed in the future)
for now the compile has some options to make your life easier

### Create New Project
To create a new Hello world project you can use `create` command

```
jot create <name>
```

It will create a new folder with the project name that has `main.jot` file with hello world sample

### Compile

Jot compile command is the most useful command for you takes the main source file path and compile it

```
jot compile <path>
```

The default output file name is `output` but you can customize it using `-o` flag

```
jot compile <path> -o <outout name>
```

The default is that jot compile report only errors but you can enable reporting warns using `-w` flag

```
jot compile <path> -w
```

The default is the the compiler will continue working if it found no error but you can make it convert warns to error using `-werr` flag

```
jot compile <path> -werr
```

### Check

Jot check command takes the source file path to parse and perform type checking then report if there are any errors

```
jot check <path>
```

### Version

Jot version command print the current langauge version in the standard out

```
jot version
```

### Help

Jot help command in a local command that show you what are the options and how to use them

```
jot help
```