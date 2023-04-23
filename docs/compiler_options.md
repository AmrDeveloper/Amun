Currently the Compiler CLI options is your best friend to interact with the compiler (this may changed in the future)
for now the compile has some options to make your life easier

### Create New Project
To create a new Hello world project you can use `create` command

```
amun create <name>
```

It will create a new folder with the project name that has `main.amun` file with hello world sample

### Compile

amun compile command is the most useful command for you takes the main source file path and compile it

```
amun compile <path> <options>
```

The default output file name is `output` but you can customize it using `-o` flag

```
amun compile <path> -o <outout name>
```

The default is that amun compile report only errors but you can enable reporting warns using `-w` flag

```
amun compile <path> -w
```

The default is the the compiler will continue working if it found no error but you can make it convert warns to error using `-werr` flag

```
amun compile <path> -werr
```

### Compile to LLVM IR File

This command is accepting the same options like the `compile` command but it produce LLVM Ir file

```
amun emit-ir <path> <options>
```

The default output file name is `output` but you can customize it using `-o` flag

```
amun compile <path> -o <outout name>
```

The default is that amun compile report only errors but you can enable reporting warns using `-w` flag

```
amun compile <path> -w
```

The default is the the compiler will continue working if it found no error but you can make it convert warns to error using `-werr` flag

### Check

amun check command takes the source file path to parse and perform type checking then report if there are any errors

```
amun check <path>
```

### Version

amun version command print the current langauge version in the standard out

```
amun version
```

### Help

amun help command in a local command that show you what are the options and how to use them

```
amun help
```