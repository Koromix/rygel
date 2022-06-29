# Supported platforms

The following combinations of OS and architectures __are officially supported and tested__ at the moment:

ISA / OS           | Windows     | Linux    | macOS       | FreeBSD     | OpenBSD
------------------ | ----------- | -------- | ----------- | ----------- | --------
x86 (IA32) [^1]    | ✅ Yes      | ✅ Yes   | ⬜️ *N/A*    | ✅ Yes      | ✅ Yes
x86_64 (AMD64)     | ✅ Yes      | ✅ Yes   | ✅ Yes      | ✅ Yes      | ✅ Yes
ARM32 LE [^2]      | ⬜️ *N/A*    | ✅ Yes   | ⬜️ *N/A*    | 🟨 Probably | 🟨 Probably
ARM64 (AArch64) LE | ✅ Yes      | ✅ Yes   | ✅ Yes      | ✅ Yes      | 🟨 Probably
RISC-V 64 [^3]     | ⬜️ *N/A*    | ✅ Yes   | ⬜️ *N/A*    | 🟨 Probably | 🟨 Probably

For all fully supported platforms (green check marks), a prebuilt binary is included in the NPM package which means you can install Koffi without a C++ compiler.

Koffi requires [Node.js](https://nodejs.org/) version 14.17.0 or newer (with [N-API](https://nodejs.org/api/n-api.html) version 8), earlier versions are not supported because N-API 7 and earlier lack several functions used by Koffi. Use [NVM](https://github.com/nvm-sh/nvm) to install more recent Node versions on older Linux distributions.

[^1]: The following call conventions are supported for forward calls: cdecl, stdcall, MS fastcall, thiscall. Only cdecl and stdcall can be used for C to JS callbacks.
[^2]: The prebuilt binary uses the hard float ABI and expects a VFP coprocessor. Build from source to use Koffi with a different ABI (softfp, soft).
[^3]: The prebuilt binary uses the LP64D (double-precision float) ABI. The LP64 ABI is supported in theory if you build Koffi from source (untested), the LP64F ABI is not supported.
