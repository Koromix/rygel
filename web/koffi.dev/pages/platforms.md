# Node.js

Koffi requires a recent [Node.js](https://nodejs.org/) version with [N-API](https://nodejs.org/api/n-api.html) version 8 support:

- Node < 12.22.0 is not supported
- _Node 12.x_: Node 12.22.0 or newer
- _Node 14.x_: Node 14.17.0 or newer
- _Node 15.x_: Node 15.12.0 or newer
- Node 16.0.0 or later versions

Use [NVM](https://github.com/nvm-sh/nvm) to install more recent Node versions on older Linux distributions.

# Supported platforms

The following combinations of OS and architectures __are officially supported and tested__ at the moment:

ISA / OS           | Windows     | Linux (glibc) | Linux (musl) | macOS       | FreeBSD     | OpenBSD
------------------ | ----------- | ------------- | ------------ | ----------- | ----------- | --------
x86 (IA32) [^1]    | ✅ Yes      | ✅ Yes        | 🟨 Probably  | ⬜️ *N/A*    | ✅ Yes      | ✅ Yes
x86_64 (AMD64)     | ✅ Yes      | ✅ Yes        | ✅ Yes       | ✅ Yes      | ✅ Yes      | ✅ Yes
ARM32 LE [^2]      | ⬜️ *N/A*    | ✅ Yes        | 🟨 Probably  | ⬜️ *N/A*    | 🟨 Probably | 🟨 Probably
ARM64 (AArch64) LE | ✅ Yes      | ✅ Yes        | ✅ Yes       | ✅ Yes      | ✅ Yes      | 🟨 Probably
RISC-V 64 [^3]     | ⬜️ *N/A*    | ✅ Yes        | 🟨 Probably  | ⬜️ *N/A*    | 🟨 Probably | 🟨 Probably
LoongArch64        | ⬜️ *N/A*    | ✅ Yes        | 🟨 Probably  | ⬜️ *N/A*    | 🟨 Probably | 🟨 Probably

<div class="legend">✅ Yes | 🟨 Probably | ⬜️ Not applicable</div>

[^1]: The following call conventions are supported for forward calls: cdecl, stdcall, MS fastcall, thiscall. Only cdecl and stdcall can be used for C to JS callbacks.
[^2]: The prebuilt binary uses the hard float ABI and expects a VFP coprocessor. Build from source to use Koffi with a different ABI (softfp, soft).
[^3]: The prebuilt binary uses the LP64D (double-precision float) ABI. The LP64 ABI is supported in theory if you build Koffi from source (untested), the LP64F ABI is not supported.

For all fully supported platforms (green check marks), a prebuilt binary is included in the NPM package which means you can install Koffi without a C++ compiler.

<style>
     table td:not(:first-child) { text-align: center; }
</style>
