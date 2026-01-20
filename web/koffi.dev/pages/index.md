# Overview

Koffi is a **fast and easy-to-use C FFI module for Node.js**, featuring:

* Low-overhead and fast performance (see [benchmarks](benchmarks))
* Support for primitive and aggregate data types (structs and fixed-size arrays), both by reference (pointer) and by value
* Javascript functions can be used as C callbacks
* Well-tested code base for popular OS/architecture combinations

If you like this project, consider supporting me:

<p style="display: flex; gap: 1em; justify-content: center; align-items: center;">
     <a href="https://liberapay.com/Koromix/donate" target="_blank"><img alt="Donate using Liberapay" src="https://liberapay.com/assets/widgets/donate.svg"></a>
     <a href="https://github.com/sponsors/koromix" target="_blank"><img src="https://img.shields.io/static/v1?label=Sponsor&message=%E2%9D%A4&logo=GitHub&color=%23fe8e86"></a>
     <a href="https://buymeacoffee.com/koromix" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/default-yellow.png" alt="Buy Me A Coffee" height="30" width="108" style="border-radius: 12px;"></a>
</p>

Koffi requires [Node.js](https://nodejs.org/) version 16 or later. Use [NVM](https://github.com/nvm-sh/nvm) to install more recent Node versions on older Linux distributions.

The following combinations of OS and architectures __are officially supported and tested__ at the moment:

ISA / OS           | Windows | Linux (glibc) | Linux (musl) | macOS | FreeBSD | OpenBSD
------------------ | ------- | ------------- | ------------ | ----- | ------- | -------
x86 (IA32) [^2]    | ✅       | ✅             | 🟨            | ⬜️    | ✅       | ✅
x86_64 (AMD64)     | ✅       | ✅             | ✅            | ✅    | ✅       | ✅
ARM32 LE [^3]      | ⬜️       | ✅             | 🟨            | ⬜️    | 🟨       | 🟨
ARM64 (AArch64) LE | ✅       | ✅             | ✅            | ✅    | ✅       | 🟨
RISC-V 64 [^4]     | ⬜️       | ✅             | 🟨            | ⬜️    | 🟨       | 🟨
LoongArch64        | ⬜️       | ✅             | 🟨            | ⬜️    | 🟨       | 🟨

<div class="legend">✅ Yes | 🟨 Probably | ⬜️ Not applicable</div>

[^2]: The following call conventions are supported for forward calls: cdecl, stdcall, MS fastcall, thiscall. Only cdecl and stdcall can be used for C to JS callbacks.
[^3]: The prebuilt binary uses the hard float ABI and expects a VFP coprocessor. Build from source to use Koffi with a different ABI (softfp, soft).
[^4]: The prebuilt binary uses the LP64D (double-precision float) ABI. The LP64 ABI is supported in theory if you build Koffi from source (untested), the LP64F ABI is not supported.

For all fully supported platforms (green check marks), a prebuilt binary is included in the NPM package which means you can install Koffi without a C++ compiler.

# Source code

The source code is available here: https://github.com/Koromix/rygel/ (in the *src/koffi* subdirectory).

> [!NOTE]
> Most of my projects live in a single repository (or monorepo), which have two killer features for me:
>
> - Cross-project refactoring
> - Simplified dependency management
>
> You can find a more detailed rationale here: https://danluu.com/monorepo/

New releases are frequent, look at the [changelog](changelog) for more information.

# License

This program is free software: you can redistribute it and/or modify it under the terms of the **MIT License**.

Find more information here: https://choosealicense.com/licenses/mit/

<style>
     table td:not(:first-child) { text-align: center; }
</style>
