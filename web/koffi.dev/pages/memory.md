# Read C values

Use `koffi.read(pointer, type)` to decode C values, wrapped as pointer objects or as simple numbers.

By default, Koffi expects NUL terminated strings when decoding them. See below if you need to specify the string length.

The following example illustrates how to decode an integer and a C string variable.

```c
#include <stddef.h>

int my_int = 42;
const char *my_string = "foobar";
```

```js
const my_int = lib.symbol('my_int', 'int');
const my_string = lib.symbol('my_string', 'const char *');

console.log(koffi.read(my_int, 'int *')) // Prints 42
console.log(koffi.read(my_string, 'const char *')) // Prints "foobar"
```

There is also an optional ending `length` argument that you can use in two cases:

- Use it to give the number of bytes to decode in non-NUL terminated strings: `koffi.decode(value, 'char *', 5)`
- Decode consecutive values into an array. For example, here is how you can decode an array with 3 float values: `koffi.decode(value, 'float', 3)`. This is equivalent to `koffi.decode(value, koffi.array('float', 3))`.

The example below will decode the symbol `my_string` defined above but only the first three bytes.

```js
// Only decode 3 bytes from the C string my_string
let str = koffi.decode(my_string, 'const char *', 3);
console.log(str); // Prints "foo"
```

# Write C values

Use `koffi.write(ptr, type, value)` to encode JS values into C symbols or pointers, wrapped as external objects or as simple numbers.

We'll reuse the examples shown above and change the variable values with `koffi.write()`.

```c
#include <stddef.h>

int my_int = 42;
const char *my_string1 = NULL;
const char *my_string2 = "FOO";
```

```js
const my_int = lib.symbol('my_int', 'int');
const my_string1 = lib.symbol('my_string1', 'const char *');
const my_string2 = lib.symbol('my_string2', 'const char *');

// Read through pointer objects
console.log(my_int.read()); // Prints 42
console.log(my_string1.read()); // Prints null
console.log(my_string2.read()); // Prints "FOO"

// Do the same but using koffi.read()
console.log(koffi.read(my_int, 'int')); // Prints 42
console.log(koffi.read(my_string1, 'const char *')); // Prints null
console.log(koffi.read(my_string2, 'const char *')); // Prints "FOO"

// Change C values
my_int.write(-1);
my_string1.write('Hello World!');
koffi.write(my_string2, 'const char *', 'BAR');

console.log(my_int.read()); // Prints -1
console.log(my_string1.read()); // Prints "Hello World!"
console.log(my_string2.read()); // Prints "BAR"
```

When encoding strings (either directly or embedded in arrays or structs), the memory will be bound to the raw pointer value and managed by Koffi. You can assign to the same string again and again without any leak or risk of use-after-free.
