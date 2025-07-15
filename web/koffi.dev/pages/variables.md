# Variable definitions

*New in Koffi 2.6*

To find an exported and  declare a variable, use `lib.symbol(name, type)`. You need to specify its name and its type.

```c
int my_int = 42;
const char *my_string = NULL;
```

```js
const my_int = lib.symbol('my_int', 'int');
const my_string = lib.symbol('my_string', 'const char *');
```

You cannot directly manipulate these variables, use:

- [koffi.decode()](#decode-to-js-values) to read their value
- [koffi.encode()](#encode-to-c-memory) to change their valuèe

# Decode to JS values

*New in Koffi 2.2, changed in Koffi 2.3*

Use `koffi.decode()` to decode C pointers, wrapped as external objects or as simple numbers.

Some arguments are optional and this function can be called in several ways:

- `koffi.decode(value, type)`: no offset
- `koffi.decode(value, offset, type)`: explicit offset to add to the pointer before decoding

By default, Koffi expects NUL terminated strings when decoding them. See below if you need to specify the string length.

The following example illustrates how to decode an integer and a C string variable.

```c
int my_int = 42;
const char *my_string = "foobar";
```

```js
const my_int = lib.symbol('my_int', 'int');
const my_string = lib.symbol('my_string', 'const char *');

console.log(koffi.decode(my_int, 'int')) // Prints 42
console.log(koffi.decode(my_string, 'const char *')) // Prints "foobar"
```

There is also an optional ending `length` argument that you can use in two cases:

- Use it to give the number of bytes to decode in non-NUL terminated strings: `koffi.decode(value, 'char *', 5)`
- Decode consecutive values into an array. For example, here is how you can decode an array with 3 float values: `koffi.decode(value, 'float', 3)`. This is equivalent to `koffi.decode(value, koffi.array('float', 3))`.

The example below will decode the symbol `my_string` defined above but only the first three bytes.

```js
// Only decode 3 bytes from the C string my_string
console.log(koffi.decode(my_string, 'const char *', 3)) // Prints "foo"
```

> [!NOTE]
> In Koffi 2.2 and earlier versions, the length argument is only used to decode strings and is ignored otherwise.

# Encode to C memory

*New in Koffi 2.6*

Use `koffi.encode()` to encode JS values into C symbols or pointers, wrapped as external objects or as simple numbers.

Some arguments are optional and this function can be called in several ways:

- `koffi.encode(ref, type, value)`: no offset
- `koffi.encode(ref, offset, type, value)`: explicit offset to add to the pointer before encoding

We'll reuse the examples shown above and change the variable values with `koffi.encode()`.

```c
int my_int = 42;
const char *my_string = NULL;
```

```js
const my_int = lib.symbol('my_int', 'int');
const my_string = lib.symbol('my_string', 'const char *');

console.log(koffi.decode(my_int, 'int')) // Prints 42
console.log(koffi.decode(my_string, 'const char *')) // Prints null

koffi.encode(my_int, 'int', -1);
koffi.encode(my_string, 'const char *', 'Hello World!');

console.log(koffi.decode(my_int, 'int')) // Prints -1
console.log(koffi.decode(my_string, 'const char *')) // Prints "Hello World!"
```

When encoding strings (either directly or embedded in arrays or structs), the memory will be bound to the raw pointer value and managed by Koffi. You can assign to the same string again and again without any leak or risk of use-after-free.

There is also an optional ending `length` argument that you can use to encode an array. For example, here is how you can encode an array with 3 float values: `koffi.encode(symbol, 'float', [1, 2, 3], 3)`. This is equivalent to `koffi.encode(symbol, koffi.array('float', 3), [1, 2, 3])`.

> [!NOTE]
> The length argument did not work correctly before Koffi 2.6.11.
