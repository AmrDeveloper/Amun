## Operator overloading

Amun supports operator overloading, some operators can be used to accomplish different tasks based on their input arguments, This is possible because operators are syntactic sugar for method calls, for example `+` opertor in a + b will converted to `add(a, b)` for those types.

There are 3 types of operator overloaded functions depending on what operator you want to override.

Note: Any overloaded operator function must has at least non primitives type

### Prefix Operator overloading

Declare prefix operator overloading function is not very almost the same like prefix function that take only one argument

Prefix operators are `!`, `~`, `++`, `--`.

```
@prefix operator !  (x Type) Type {}

@prefix operator ~  (x Type) Type {}

@prefix operator ++ (x Type) Type {}

@prefix operator -- (x Type) Type {}
```

### Infix Operator overloading

Infix operators are `+`, `-`, `+`, `/`, `%`, `==`, `!=`, `>=`, `<=`, `>`, `<`, `<<`, `>>`, `&`, `|`, `&&`, `||`.

```
operator +  (x Type, y Type) Type {}
operator -  (x Type, y Type) Type {}
operator *  (x Type, y Type) Type {}
operator /  (x Type, y Type) Type {}
operator %  (x Type, y Type) Type {}

operator == (x Type, y Type) Type {}
operator != (x Type, y Type) Type {}
operator >= (x Type, y Type) Type {}
operator <= (x Type, y Type) Type {}

operator << (x Type, y Type) Type {}
operator >> (x Type, y Type) Type {}

operator &  (x Type, y Type) Type {}
operator |  (x Type, y Type) Type {}

operator && (x Type, y Type) Type {}
operator || (x Type, y Type) Type {}
```

Note: You can add `@infix` before operator declaraion but it default and no need to explicit write it

### Postfix Operator overloading

Declare postfix operator overloading function is not very almost the same like postfix function that take only one argument

Postfix operators are `!`, `~`, `++`, `--`.

```
@postfix operator ++ (x Type) Type {}

@postfix operator -- (x Type) Type {}
```