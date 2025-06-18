# Node.js

Koffi requires _Node.js 16_ or a more recent version.

Use [NVM](https://github.com/nvm-sh/nvm) to install recent Node versions on old Linux distributions.

# Platforms

The following combinations of OS and architectures __are officially supported and tested__ at the moment:

ISA / OS           | Windows | Linux (glibc) | Linux (musl)
------------------ | ------- | ------------- | ------------
x86 (IA32) [^1]    | ✅      | ✅            | 🟨
x86_64 (AMD64)     | ✅      | ✅            | ✅
ARM32 LE [^2]      | ⬜️      | ✅            | 🟨
ARM64 (AArch64) LE | ✅      | ✅            | 🟨
RISC-V 64 [^3]     | ⬜️      | ✅            | 🟨
LoongArch64        | ⬜️      | ✅            | 🟨

<div class="legend">✅ Yes | 🟨 Probably | ⬜️ Not applicable</div>

ISA / OS           | macOS | FreeBSD     | OpenBSD
------------------ | ----- | ----------- | --------
x86 (IA32) [^1]    | ⬜️    | ✅          | ✅
x86_64 (AMD64)     | ✅    | ✅          | ✅
ARM32 LE [^2]      | ⬜️    | 🟨          | 🟨
ARM64 (AArch64) LE | ✅    | ✅          | 🟨
RISC-V 64 [^3]     | ⬜️    | 🟨          | 🟨
LoongArch64        | ⬜️    | 🟨          | 🟨

<div class="legend">✅ Yes | 🟨 Probably | ⬜️ Not applicable</div>

[^1]: The following call conventions are supported for forward calls: cdecl, stdcall, MS fastcall, thiscall. Only cdecl and stdcall can be used for C to JS callbacks.
[^2]: The prebuilt binary uses the hard float ABI and expects a VFP coprocessor. Build from source to use Koffi with a different ABI (softfp, soft).
[^3]: The prebuilt binary uses the LP64D (double-precision float) ABI. The LP64 ABI is supported in theory if you build Koffi from source (untested), the LP64F ABI is not supported.

For all fully supported platforms (green check marks), a prebuilt binary is included in the NPM package which means you can install Koffi without a C++ compiler.

<style>
     table td:not(:first-child) { text-align: center; }
</style>
