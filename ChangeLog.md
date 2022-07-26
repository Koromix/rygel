# Changelog

## Koffi 1.3.11

**Main fixes:**

- Fix broken parsing of `void *` when used for first parameter

## Koffi 1.3.10

**Main fixes:**

- Fix support for callbacks with more than 4 parameters on Windows x64
- Fix support for callbacks with multiple floating-point arguments on ARM32 platforms
- Fix possibly incorrect conversion for uint32_t callback parameters

**Other changes:**

- Various documentation fixes and improvements

## Koffi 1.3.9

**Main fixes:**

- Fix prebuild compatibility with Electron on Windows x64

## Koffi 1.3.8

**Main changes:**

- Prevent callback reuse beyond FFI call
- Add BTI support for AAarch64 platforms (except Windows)

**Other changes:**

- Fix and harmonize a few error messages

## Koffi 1.3.7

**Main fixes:**

- Fix crash when using callbacks inside structs
- Support for null strings in record members

**Other changes:**

- Add intptr_t and uintptr_t primitive types
- Add str/str16 type aliases for string/string16
- Various documentation fixes and improvements

## Koffi 1.3.6

**Main fixes:**

- Fix install error with Node < 15 on Windows (build system bug)

**Other changes:**

- Detect incompatible Node.js versions when installing Koffi
- Prebuild with Clang for Windows x64 and Linux x64 binaries
- Various documentation improvements

## Koffi 1.3.5

**Main changes:**

- Fix memory leak when many async calls are running
- Add configurable limit for maximum number of async calls (max_async_calls)

**Other changes:**

- Reduce default async memory stack and heap size
- Various documentation improvements

## Koffi 1.3.4

**Main fixes:**

- Fix possible OpenBSD i386 crash with `(void)` functions

## Koffi 1.3.3

**Main fixes:**

- Fix misconversion of signed integer return value as unsigned

**Other changes:**

- Support `(void)` (empty) function signatures
- Disable unsafe compiler optimizations
- Various documentation improvements

## Koffi 1.3.2

**Main fixes:**

- Support compilation in C++14 mode (graceful degradation)
- Support older toolchains on Linux (tested on Debian 9)

## Koffi 1.3.1

**Main fixes:**

- The prebuilt binary is tested when Koffi is installed, and a rebuild happens if it fails to load

## Koffi 1.3.0

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

## Koffi 1.2.4

**New features:**

- Windows ARM64 is now supported

## Koffi 1.2.3

**New features:**

- A prebuilt binary for macOS ARM64 (M1) is now included

## Koffi 1.2.1

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
