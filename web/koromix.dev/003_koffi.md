<!-- Title: koromix.dev — Koffi
     Menu: Koffi
     Created: 2022-05-16 -->

#overview# Overview

Koffi is a fast and easy-to-use C FFI module for [Node.js](https://nodejs.org/), featuring:

* Low-overhead and fast performance (see [benchmarks](https://github.com/Koromix/luigi/tree/master/koffi#benchmarks))
* Support for primitive and aggregate data types (structs and fixed-size arrays), both by reference (pointer) and by value
* Javascript functions can be used as C callbacks (since 1.2.0)
* Well-tested code base for popular OS/architecture combinations

The following combinations of OS and architectures __are officially supported and tested__ at the moment:

ISA / OS           | Windows     | Linux    | macOS       | FreeBSD     | OpenBSD
------------------ | ----------- | -------- | ----------- | ----------- | --------
x86 (IA32)         | ✅ Yes      | ✅ Yes   | ⬜️ *N/A*    | ✅ Yes      | ✅ Yes
x86_64 (AMD64)     | ✅ Yes      | ✅ Yes   | ✅ Yes      | ✅ Yes      | ✅ Yes
ARM32 LE           | ⬜️ *N/A*    | ✅ Yes   | ⬜️ *N/A*    | 🟨 Probably | 🟨 Probably
ARM64 (AArch64) LE | 🟧 Maybe    | ✅ Yes   | ✅ Yes      | ✅ Yes      | 🟨 Probably
RISC-V 64          | ⬜️ *N/A*    | ✅ Yes   | ⬜️ *N/A*    | 🟨 Probably | 🟨 Probably

You can find more information about Koffi on the official web site: [https://koffi.dev/](https://koffi.dev)

#license# License

This program is free software: you can redistribute it and/or modify it under the terms of the **GNU Affero General Public License** as published by the Free Software Foundation, either **version 3 of the License**, or (at your option) any later version.

Find more information here: [https://www.gnu.org/licenses/](https://www.gnu.org/licenses/)
