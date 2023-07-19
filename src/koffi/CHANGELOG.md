# Changelog

## Version history

### Koffi 2.5

#### Koffi 2.5.7

**Main changes:**

- Point package to new repository

#### Koffi 2.5.6

**Main changes:**

- Increase limit of output parameters from 16 to 32

#### Koffi 2.5.5

**Main changes:**

- Support decoding non-char null-terminated arrays

#### Koffi 2.5.4

**Main changes:**

- Fix `koffi.pointer()` not accepting disposable types

**Other changes:**

- Fix potential issues when making pointers to anonymous types

#### Koffi 2.5.3

**Main changes:**

- Add missing union exports in TS definition file

**Other changes:**

- Fix some parameter names in TS definition file

#### Koffi 2.5.2

**Main changes:**

- Default initialize unset object/struct members

#### Koffi 2.5.1

**Main changes:**

- Fix crash with some struct types in System V 64 ABI
- Always use direct passthrough for buffer arguments

#### Koffi 2.5.0

**New features:**

- Support [union types](unions.md)

**Other fixes:**

- Fix ABI for single-float aggregate return on i386 BSD systems
- Don't mess with Node.js signal handling on POSIX systems

### Koffi 2.4

#### Koffi 2.4.2

**Main changes:**

- Support calling variadic function pointers

**Other changes:**

- Add documentation for function pointers

#### Koffi 2.4.1

**Main changes:**

- Support [decoding function pointers](functions.md#decode-pointer-to-function) to callable functions
- Support calling function pointers with [koffi.call()](functions.md#call-pointer-directly)
- Deprecate `koffi.callback` in favor of `koffi.proto`

**Other changes:**

- Increase maximum number of callbacks to 8192
- Build Koffi with static CRT on Windows
- Reorganize Koffi data conversion documentation
- Add deprecated `koffi.handle` to TS file

### Koffi 2.3

#### Koffi 2.3.20

**Main fixes:**

- Support explicit library unloading with `lib.unload()`

#### Koffi 2.3.19

**Main fixes:**

- Actually allow non-ambiguous [string] values for `void *` arguments

#### Koffi 2.3.18

**Main fixes:**

- Fix possible crash on exit caused by unregistered callbacks

#### Koffi 2.3.17

**Main changes:**

- Allow strings for input `void *`, `int8_t *` and `int16_t *` pointer arguments
- Support using `[string]` (single-element string arrays) for polymorphic input/output arguments

#### Koffi 2.3.16

**Main changes:**

- Fix Windows ARM64 build to work with official Node.js version
- Compile Windows builds with Visual Studio 2022 17.5.3

**Other changes:**

- Support null in `koffi.free()` and `koffi.address()`

#### Koffi 2.3.15

**Main changes:**

- Improve manual decoding of 0-terminated strings
- Add checks around array conversion hints

#### Koffi 2.3.14

**Main changes:**

- Add `koffi.errno()` function to get and set current errno value
- Add `koffi.os.errno` object with valid errno codes

#### Koffi 2.3.13

**Main changes:**

- Add `koffi.address()` to get the raw value of a wrapper pointer

#### Koffi 2.3.12

**Main fixes:**

- Fix broken syntax in TS definition file
- Add missing exported properties in TS file

#### Koffi 2.3.11

**Main changes:**

- Allow numbers and BigInts to be used for pointer arguments

**Other changes:**

- Use SQLITE_TRANSIENT in SQLite test code
- Avoid using `statx()` to allow compilation with glibc < 2.28
- Reorganize NPM package files to be less convoluted

#### Koffi 2.3.9

**Main changes:**

- Relicense under MIT license

#### Koffi 2.3.8

**Main fixes:**

- Disable non-ready union support
- Simplify Windows stack allocation and drop NOACCESS and GUARD pages
- Adjust Windows TEB SEH chain and GuaranteedStackBytes for Koffi calls

#### Koffi 2.3.7

**Main fixes:**

- Fix missing require in index.js ([@gastonFrecceroNapse](https://github.com/gastonFrecceroNapse))
- Reduce NPM package bloat (from 65 MB to 20 MB) caused by changes in 2.3.6

#### Koffi 2.3.6

**Main changes:**

- Fix broken TS definition file
- Keep all prebuilt binaries and load correct one at runtime

#### Koffi 2.3.5

**Main fixes:**

- Fix rare random crashes with async callbacks
- Fix some bugs with RISC-V 64 ABI

**Other changes:**

- Expose array type hint in `koffi.introspect()`
- Add missing `koffi.array()` export in TS definition
- Make KoffiFunction more flexible in TS definition ([@insraq](https://github.com/insraq))
- Add a KoffiFunc<T> helper type in TS typing ([@insraq](https://github.com/insraq))
- Mark optional properties in TS TypeInfo class
- Minor performance improvements

#### Koffi 2.3.4

**Main fixes:**

- Fix error when installing Koffi on Windows (2.3.2)

#### Koffi 2.3.2

**Main changes:**

- Fix type parser issues (such as ignoring some [disposable qualifiers](parameters.md#heap-allocated-values))
- Fix crash when using disposable types in output parameters
- Add basic [koffi.stats()](misc.md#usage-statistics) interface

**Other changes:**

- Avoid CNoke dependency
- Clear out development dependencies from package.json

#### Koffi 2.3.1

**Main changes:**

- Error out when trying to use ambiguous `void *` arguments (input and/or output)
- Adjust TypeScript definitions ([@insraq](https://github.com/insraq))
- Fix possible crash when parsing invalid prototype

#### Koffi 2.3.0

**Main changes:**

- Allow buffers (TypedArray or ArrayBuffer) values for input and/or output pointer arguments (for polymorphic arguments)
- Support opaque buffers (TypedArray or ArrayBuffer) values in `koffi.decode()` to [decode output buffers](parameters.md#output-buffers)
- Decode non-string types as arrays when an [explicit length is passed to koffi.decode()](callbacks.md#decoding-pointer-arguments)

**Other changes:**

- Drop TypedArray type check for array and pointer values (only the size matters)
- Allow ArrayBuffer everywhere TypedArray is supported
- Add TypeScript definition for Koffi ([@insraq](https://github.com/insraq))
- Improve documentation about opaque and polymorphic structs
- Improve documentation for `koffi.decode()`
- Reduce NPM package size (from 100 MB to 25 MB) by removing test code and dependencies

### Koffi 2.2

#### Koffi 2.2.5

**Main changes:**

- Relicense Koffi under LGPL 3.0

#### Koffi 2.2.4

**Main fixes:**

- Fix memory leak on Windows (in Koffi 2.2.3) when running many async calls

**Other changes:**

- Reorganize documentation pages

#### Koffi 2.2.3

**Main fixes:**

- Support native code that uses [Structured Exception Handling (SEH)](https://learn.microsoft.com/en-us/cpp/cpp/structured-exception-handling-c-cpp) on Windows (x86, x64 and ARM64)

**Other changes:**

- Try to use ebp/rbp as frame pointer in x86/x64 ASM code

#### Koffi 2.2.2

**Main fixes:**

- Support transparent [asynchronous callbacks](callbacks.md#asynchronous-callbacks)
- Expand from a maximum of 16+16 to 1024 callbacks running in parallel

**Other fixes:**

- Fix bundler support by removing shebang from index.js
- Fix bugs when loading Koffi multiples times in same process (context aware module)
- Check N-API version when module is loaded
- Optimize callback unregistration

#### Koffi 2.2.1

**Main fixes:**

- Fix crash when [calling callback again after FFI call inside previous callback](https://github.com/Koromix/rygel/issues/15)

#### Koffi 2.2.0

**New features:**

- Add [koffi.decode()](callbacks.md#decoding-pointer-arguments) for callback pointer arguments
- Support transparent [output string parameters](parameters.md#output-parameters)
- Add `koffi.offsetof()` utility function
- Support optional *this* binding in `koffi.register()`

**Other fixes:**

- Correctly validate output parameter types
- Fix assertion with `void *` parameters

### Koffi 2.1

#### Koffi 2.1.5

**Main fixes:**

- Add missing README.md file to NPM package

#### Koffi 2.1.4

**Main changes:**

- Increase maximum type size from 32 kiB to 64 MiB
- Add configurable option for maximum type size

**Other changes:**

- Moved Koffi to a new repository: https://github.com/Koromix/rygel/

#### Koffi 2.1.3

**Main changes:**

- Support up to 16 output parameters (instead of 8)

#### Koffi 2.1.2

**Main changes:**

- Support up to 8 output parameters (instead of 4)

#### Koffi 2.1.1

**Main fixes:**

- Fix potential memory allocation bugs

#### Koffi 2.1.0

**Main changes:**

- Add [koffi.as()](parameters.md#polymorphic-parameters) to support polymorphic APIs based on `void *` parameters
- Add [endian-sensitive integer types](types.md#endian-sensitive-types): `intX_le_t`, `intX_be_t`, `uintX_le_t`, `uintX_be_t`
- Accept typed arrays for `void *` parameters
- Introduce `koffi.opaque()` to replace `koffi.handle()` (which remains supported until Koffi 3.0)
- Support JS Array and TypedArray to fill struct and array pointer members

**Other changes:**

- Improve global performance with inlining and unity builds
- Add `size_t` primitive type
- Support member-specific alignement values in structs
- Detect impossible parameter and return types (such as non-pointer opaque types)
- Various documentation fixes and improvements

### Koffi 2.0

#### Koffi 2.0.1

**Main changes:**

- Return `undefined` (instead of null) for `void` functions

#### Koffi 2.0.0

**Major new features:**

- Add [disposable types](parameters.md#heap-allocated-values) for automatic disposal of C values (such as heap-allocated strings)
- Add support for [registered callbacks](callbacks.md#registered-callbacks), that can be called after the initial FFI call
- Support named pointer types
- Support complex type specifications outside of prototype parser

**Minor new features:**

- Support type aliases with `koffi.alias()`
- Add `koffi.resolve()` to resolve type strings
- Expose all primitive type aliases in `koffi.types`
- Correctly pass exceptions thrown in JS callbacks

**Breaking API changes:**

- Change handling of callback types, which must be used through pointers
- Change handling of opaque handles, which must be used through pointers
- Support all types in `koffi.introspect(type)`

Consult the [migration guide](migration.md) for more information.

### Koffi 1.3

#### Koffi 1.3.12

**Main fixes:**

- Fix support for Yarn package manager

#### Koffi 1.3.11

**Main fixes:**

- Fix broken parsing of `void *` when used for first parameter

#### Koffi 1.3.10

**Main fixes:**

- Fix support for callbacks with more than 4 parameters on Windows x64
- Fix support for callbacks with multiple floating-point arguments on ARM32 platforms
- Fix possibly incorrect conversion for uint32_t callback parameters

**Other changes:**

- Various documentation fixes and improvements

#### Koffi 1.3.9

**Main fixes:**

- Fix prebuild compatibility with Electron on Windows x64

#### Koffi 1.3.8

**Main changes:**

- Prevent callback reuse beyond FFI call
- Add BTI support for AAarch64 platforms (except Windows)

**Other changes:**

- Fix and harmonize a few error messages

#### Koffi 1.3.7

**Main fixes:**

- Fix crash when using callbacks inside structs
- Support for null strings in record members

**Other changes:**

- Add intptr_t and uintptr_t primitive types
- Add str/str16 type aliases for string/string16
- Various documentation fixes and improvements

#### Koffi 1.3.6

**Main fixes:**

- Fix install error with Node < 15 on Windows (build system bug)

**Other changes:**

- Detect incompatible Node.js versions when installing Koffi
- Prebuild with Clang for Windows x64 and Linux x64 binaries
- Various documentation improvements

#### Koffi 1.3.5

**Main changes:**

- Fix memory leak when many async calls are running
- Add configurable limit for maximum number of async calls (max_async_calls)

**Other changes:**

- Reduce default async memory stack and heap size
- Various documentation improvements

#### Koffi 1.3.4

**Main fixes:**

- Fix possible OpenBSD i386 crash with `(void)` functions

#### Koffi 1.3.3

**Main fixes:**

- Fix misconversion of signed integer return value as unsigned

**Other changes:**

- Support `(void)` (empty) function signatures
- Disable unsafe compiler optimizations
- Various documentation improvements

#### Koffi 1.3.2

**Main fixes:**

- Support compilation in C++14 mode (graceful degradation)
- Support older toolchains on Linux (tested on Debian 9)

#### Koffi 1.3.1

**Main fixes:**

- The prebuilt binary is tested when Koffi is installed, and a rebuild happens if it fails to load

#### Koffi 1.3.0

**Major changes:**

- Expand and move documentation to https://koffi.dev/
- Support JS arrays and TypedArrays for pointer arguments (input, output and mixed)

**Other changes:**

- Convert NULL string pointers to null instead of crashing (return values, struct and array members, callbacks)
- Default to 'string' array hint for char, char16 and char16_t arrays
- Fix definition of long types on Windows x64 (LLP64 model)
- Restrict automatic string conversion to signed char types
- Detect floating-point ABI before using prebuilt binaries (ARM32, RISC-V)
- Forbid duplicate member names in struct types

### Koffi 1.2

#### Koffi 1.2.4

**New features:**

- Windows ARM64 is now supported

#### Koffi 1.2.3

**New features:**

- A prebuilt binary for macOS ARM64 (M1) is now included

#### Koffi 1.2.1

This entry documents changes since version 1.1.0.

**New features:**

- JS functions can be used as C callbacks (cdecl, stdcall) on all platforms
- RISC-V 64 LP64D ABI is supported (LP64 is untested)
- Expose settings for memory usage of synchronous and asynchronous calls
- Transparent conversion between C buffers and strings
- Tentative support for Windows ARM64 (untested)

**Main fixes:**

- Fix excessive stack alignment of structs on x86 platforms
- Fix potential problems with big int64_t/uint64_t values
- Fix possible struct layout errors in push/pop code
- Fix alignment issues in ARM32 push code
- Fix incomplete/buggy support for HFA structs on ARM32 and ARM64
- Fix crashes on OpenBSD caused by missing MAP_STACK flag
- Fix non-sense "duplicate array type" errors
- Fix value `koffi.internal` to false in module (normal) builds
- Make sure we have a redzone below the stack for all architectures
- Use slower allocation for big objects instead of failing

## Todo list

The following features and improvements are planned, not necessarily in that order:

- Optimize passing of structs and arrays (avoid setting named properties one by one? separate HFA-specific helper functions?)
- Automate Windows/AArch64 (qemu) and macOS/AArch64 (how? ... thanks Apple) tests
- Create a real-world example, using several libraries (Raylib, SQLite, libsodium) to illustrate various C API styles
- Add simple struct type parser
- Add more ways to manually encode and decode various types to and from byte arrays
- Add support for unions
- Port Koffi to PowerPC (POWER9+) ABI
- Fix assembly unwind and CFI directives for better debugging experience
