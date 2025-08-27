# Overview

Koffi is a **fast and easy-to-use C to JS FFI module** for [Node.js](https://nodejs.org/), featuring:

* Low-overhead and fast performance (see [benchmarks](https://koffi.dev/benchmarks))
* Support for primitive and aggregate data types (structs and fixed-size arrays), both by reference (pointer) and by value
* Javascript functions can be used as C callbacks
* Well-tested code base for popular OS/architecture combinations

<div class="illustrations">
    <img src="{{ ASSET static/koffi/logo.webp }}" width="185" height="99" alt=""/>
</div>

You can find more information about Koffi on the official web site: https://koffi.dev/

# Platforms

The following combinations of OS and architectures __are officially supported and tested__ at the moment:

ISA / OS           | Windows | Linux (glibc) | Linux (musl) | macOS | FreeBSD | OpenBSD
------------------ | ------- | ------------- | ------------ | ----- | ------- | -------
x86 (IA32) [^1]    | ✅       | ✅             | 🟨            | ⬜️    | ✅       | ✅
x86_64 (AMD64)     | ✅       | ✅             | ✅            | ✅    | ✅       | ✅
ARM32 LE [^2]      | ⬜️       | ✅             | 🟨            | ⬜️    | 🟨       | 🟨
ARM64 (AArch64) LE | ✅       | ✅             | ✅            | ✅    | ✅       | 🟨
RISC-V 64 [^3]     | ⬜️       | ✅             | 🟨            | ⬜️    | 🟨       | 🟨
LoongArch64        | ⬜️       | ✅             | 🟨            | ⬜️    | 🟨       | 🟨

<div class="legend">✅ Yes | 🟨 Probably | ⬜️ Not applicable</div>

[^1]: The following call conventions are supported for forward calls: cdecl, stdcall, MS fastcall, thiscall. Only cdecl and stdcall can be used for C to JS callbacks.
[^2]: The prebuilt binary uses the hard float ABI and expects a VFP coprocessor. Build from source to use Koffi with a different ABI (softfp, soft).
[^3]: The prebuilt binary uses the LP64D (double-precision float) ABI. The LP64 ABI is supported in theory if you build Koffi from source (untested), the LP64F ABI is not supported.

# License

This program is free software: you can redistribute it and/or modify it under the terms of the **MIT License**.

Find more information here: https://choosealicense.com/licenses/mit/

<style>
     table td:not(:first-child) { text-align: center; }
</style>
