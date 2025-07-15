# Union definition

*New in Koffi 2.5*

You can declare unions with a syntax similar to structs, but with the `koffi.union()` function. This function takes two arguments: the first one is the name of the type, and the second one is an object containing the union member names and types. You can omit the first argument to declare an anonymous union.

The following example illustrates how to declare the same union in C and in JS with Koffi:

```c
typedef union IntOrDouble {
    int64_t i;
    double d;
} IntOrDouble;
```

```js
const IntOrDouble = koffi.union('IntOrDouble', {
    i: 'int64_t',
    d: 'double'
});
```

# Input unions

## Passing union values to C

You can instantiate an union object with `koffi.Union(type)`. This will create a special object that contains at most one active member.

Once you have created an instance of your union, you can simply set the member with the dot operator as you would with a basic object. Then, simply pass your union value to the C function you wish.

```js
const U = koffi.union('U', { i: 'int', str: 'char *' });

const DoSomething = lib.func('void DoSomething(const char *type, U u)');

const u1 = new koffi.Union('U'); u1.i = 42;
const u2 = new koffi.Union('U'); u2.str = 'Hello!';

DoSomething('int', u1);
DoSomething('string', u2);
```

For simplicity, Koffi also accepts object literals with one property (no more, no less) setting the corresponding union member. The example belows uses this to simplify the code shown above:

```js
const U = koffi.union('U', { i: 'int', str: 'char *' });

const DoSomething = lib.func('void DoSomething(const char *type, U u)');

DoSomething('int', { i: 42 });
DoSomething('string', { str: 'Hello!' });
```

## Win32 example

The following example uses the [SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput) Win32 API to emit the Win+D shortcut and hide windows (show the desktop).

```js
// ES6 syntax: import koffi from 'koffi';
const koffi = require('koffi');

// Win32 type and functions

const user32 = koffi.load('user32.dll');

const INPUT_MOUSE = 0;
const INPUT_KEYBOARD = 1;
const INPUT_HARDWARE = 2;

const KEYEVENTF_KEYUP = 0x2;
const KEYEVENTF_SCANCODE = 0x8;

const VK_LWIN = 0x5B;
const VK_D = 0x44;

const MOUSEINPUT = koffi.struct('MOUSEINPUT', {
    dx: 'long',
    dy: 'long',
    mouseData: 'uint32_t',
    dwFlags: 'uint32_t',
    time: 'uint32_t',
    dwExtraInfo: 'uintptr_t'
});
const KEYBDINPUT = koffi.struct('KEYBDINPUT', {
    wVk: 'uint16_t',
    wScan: 'uint16_t',
    dwFlags: 'uint32_t',
    time: 'uint32_t',
    dwExtraInfo: 'uintptr_t'
});
const HARDWAREINPUT = koffi.struct('HARDWAREINPUT', {
    uMsg: 'uint32_t',
    wParamL: 'uint16_t',
    wParamH: 'uint16_t'
});

const INPUT = koffi.struct('INPUT', {
    type: 'uint32_t',
    u: koffi.union({
        mi: MOUSEINPUT,
        ki: KEYBDINPUT,
        hi: HARDWAREINPUT
    })
});

const SendInput = user32.func('unsigned int __stdcall SendInput(unsigned int cInputs, INPUT *pInputs, int cbSize)');

// Show/hide desktop with Win+D shortcut

let events = [
    make_keyboard_event(VK_LWIN, true),
    make_keyboard_event(VK_D, true),
    make_keyboard_event(VK_D, false),
    make_keyboard_event(VK_LWIN, false)
];

SendInput(events.length, events, koffi.sizeof(INPUT));

// Utility

function make_keyboard_event(vk, down) {
    let event = {
        type: INPUT_KEYBOARD,
        u: {
            ki: {
                wVk: vk,
                wScan: 0,
                dwFlags: down ? 0 : KEYEVENTF_KEYUP,
                time: 0,
                dwExtraInfo: 0
            }
        }
    };

    return event;
}
```

# Output unions

Unlike structs, Koffi does not know which union member is valid, and it cannot decode it automatically. You can however use special `koffi.Union` objects for output parameters, and decode the memory after the call.

To decode an output union pointer parameter, create a placeholder object with `new koffi.Union(type)` and pass the resulting object to the function.

After the call, you can dereference the member value you want on this object and Koffi will decode it at this moment.

The following example illustrates the use of `koffi.Union()` to decode output unions after the call.

```c
#include <stdint.h>

typedef union IntOrDouble {
    int64_t i;
    double d;
} IntOrDouble;

void SetUnionInt(int64_t i, IntOrDouble *out)
{
    out->i = i;
}

void SetUnionDouble(double d, IntOrDouble *out)
{
    out->d = d;
}
```

```js
const IntOrDouble = koffi.union('IntOrDouble', {
    i: 'int64_t',
    d: 'double',
    raw: koffi.array('uint8_t', 8)
});

const SetUnionInt = lib.func('void SetUnionInt(int64_t i, _Out_ IntOrDouble *out)');
const SetUnionDouble = lib.func('void SetUnionDouble(double d, _Out_ IntOrDouble *out)');

let u1 = new koffi.Union('IntOrDouble');
let u2 = new koffi.Union('IntOrDouble');

SetUnionInt(123, u1);
SetUnionDouble(123, u2);

console.log(u1.i, '---', u1.raw); // Prints 123 --- Uint8Array(8) [123, 0, 0, 0, 0, 0, 0, 0]
console.log(u2.d, '---', u2.raw); // Prints 123 --- Uint8Array(8) [0, 0, 0, 0, 0, 0, 69, 64]
```
