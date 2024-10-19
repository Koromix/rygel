# Node.js

Koffi requires _Node.js 16_ or a more recent version.

Use [NVM](https://github.com/nvm-sh/nvm) to install recent Node versions on old Linux distributions.

# Platforms

The following combinations of OS and architectures __are officially supported and tested__ at the moment:

ISA / OS           | Windows     | Linux (glibc) | Linux (musl) | macOS       | FreeBSD     | OpenBSD
------------------ | ----------- | ------------- | ------------ | ----------- | ----------- | --------
x86 (IA32) [^1]    | ✅ Yes      | ✅ Yes        | 🟨 Probably  | ⬜️ *N/A*    | ✅ Yes      | ✅ Yes
x86_64 (AMD64)     | ✅ Yes      | ✅ Yes        | ✅ Yes       | ✅ Yes      | ✅ Yes      | ✅ Yes
ARM32 LE [^2]      | ⬜️ *N/A*    | ✅ Yes        | 🟨 Probably  | ⬜️ *N/A*    | 🟨 Probably | 🟨 Probably
ARM64 (AArch64) LE | ✅ Yes      | ✅ Yes        | ✅ Yes       | ✅ Yes      | ✅ Yes      | 🟨 Probably
RISC-V 64 [^3]     | ⬜️ *N/A*    | ✅ Yes        | 🟨 Probably  | ⬜️ *N/A*    | 🟨 Probably | 🟨 Probably

[^1]: The following call conventions are supported for forward calls: cdecl, stdcall, MS fastcall, thiscall. Only cdecl and stdcall can be used for C to JS callbacks.
[^2]: The prebuilt binary uses the hard float ABI and expects a VFP coprocessor. Build from source to use Koffi with a different ABI (softfp, soft).
[^3]: The prebuilt binary uses the LP64D (double-precision float) ABI. The LP64 ABI is supported in theory if you build Koffi from source (untested), the LP64F ABI is not supported.

For all fully supported platforms (green check marks), a prebuilt binary is included in the NPM package which means you can install Koffi without a C++ compiler.
