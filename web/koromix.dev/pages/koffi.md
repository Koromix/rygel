# Overview

Koffi is a **fast and easy-to-use C to JS FFI module** for [Node.js](https://nodejs.org/), featuring:

* Low-overhead and fast performance (see [benchmarks](https://koffi.dev/benchmarks))
* Support for primitive and aggregate data types (structs and fixed-size arrays), both by reference (pointer) and by value
* Javascript functions can be used as C callbacks (since 1.2.0)
* Well-tested code base for popular OS/architecture combinations

<p style="text-align: center; margin: 2em;">
    <img src="{{ ASSET static/koffi/koffi.png }}" width="128" height="128" alt=""/>
</p>

You can find more information about Koffi on the official web site: https://koffi.dev/

# Platforms

The following combinations of OS and architectures __are officially supported and tested__ at the moment:

<table>
     <thead>
          <tr><th>ISA / OS</th><th>Windows</th><th>Linux</th><th>macOS</th></tr>
     </thead>
     <tbody>
          <tr><td>x86 (IA32)</td><td class="center">✅</td><td class="center">✅</td><td class="center">⬜️</td></tr>
          <tr><td>x86_64 (AMD64)</td><td class="center">✅</td><td class="center">✅</td><td class="center">✅</td></tr>
          <tr><td>ARM32 Little Endian</td><td class="center">⬜️</td><td class="center">✅</td> <td class="center">⬜️</td></tr>
          <tr><td>ARM64 (AArch64) Little Endian</td><td class="center">✅</td><td class="center">✅</td><td class="center">✅</td></tr>
          <tr><td>RISC-V 64</td><td class="center">⬜️</td><td class="center">✅</td><td class="center">⬜️</td></tr>
     </tbody>
</table>
<div class="legend">✅ Yes | 🟨 Probably | ⬜️ *Not applicable*</div>

<table>
     <thead>
          <tr><th>ISA / OS</th><th>FreeBSD</th><th>OpenBSD</th></tr>
     </thead>
     <tbody>
          <tr><td>x86 (IA32)</td><td class="center">✅</td><td class="center">✅</td></tr>
          <tr><td>x86_64 (AMD64)</td><td class="center">✅</td><td class="center">✅</td></tr>
          <tr><td>ARM32 Little Endian</td><td class="center">🟨</td><td class="center">🟨</td></tr>
          <tr><td>ARM64 (AArch64) Little Endian</td><td class="center">✅</td><td class="center">🟨</td></tr>
          <tr><td>RISC-V 64</td><td class="center">🟨</td><td class="center">🟨</td></tr>
     </tbody>
</table>
<div class="legend">✅ Yes | 🟨 Probably | ⬜️ *Not applicable*</div>

# License

This program is free software: you can redistribute it and/or modify it under the terms of the **MIT License**.

Find more information here: https://choosealicense.com/licenses/mit/
