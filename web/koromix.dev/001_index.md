<!-- Title: koromix.dev
     Menu: Home
     Created: 2017-01-15 -->

#tytools# TyTools

TyTools is a collection of **independent tools** and you only need one executable to use
any of them. The Qt-based GUI tools are statically compiled to make that possible.

Tool        | Type                      | Description
----------- | --------------------------------------------------------------------------------
TyCommander | Qt GUI (static)           | Upload, monitor and communicate with multiple boards
TyUpdater   | Qt GUI (static)           | Simple firmware / sketch uploader
tycmd       | Command-line<br>_No Qt !_ | Command-line tool to manage Teensy boards

Find out more on the [page dedicated to TyTools](tytools).

#libhs# libhs

libhs is a cross-platform C library to enumerate **HID and serial devices** and interact with them.

It is distributed as a **single-file header**. Drop the file in your project, add one define for the implementation and start to code without messing around with an arcane build system.

Find out more on the [page dedicated to libhs](libhs).

#koffi# Koffi

Koffi is a fast and easy-to-use FFI module for [Node.js](https://nodejs.org/), with support for complex data types such as structs.

The following combinations of OS and architectures __are officially supported and tested__ at the moment:

ISA / OS           | Windows     | Linux    | macOS       | FreeBSD     | OpenBSD
------------------ | ----------- | -------- | ----------- | ----------- | --------
x86 (IA32)         | 🟩 Yes      | 🟩 Yes   | ⬜️ *N/A*    | 🟩 Yes      | 🟩 Yes
x86_64 (AMD64)     | 🟩 Yes      | 🟩 Yes   | 🟩 Yes      | 🟩 Yes      | 🟩 Yes
ARM32 LE           | ⬜️ *N/A*    | 🟩 Yes   | ⬜️ *N/A*    | 🟨 Probably | 🟨 Probably
ARM64 (AArch64) LE | 🟧 Maybe    | 🟩 Yes   | 🟩 Yes      | 🟩 Yes      | 🟨 Probably
RISC-V 64          | ⬜️ *N/A*    | 🟩 Yes   | ⬜️ *N/A*    | 🟨 Probably | 🟨 Probably

Find out more on the [page dedicated to Koffi](koffi).

#misc# Other projects

The projects on this page are provided as-is, and are in **various states of completion**.

Project | Description                                                                                                                                 | Language
------- | ------------------------------------------------------------------------------------------------------------------------------------------- | --------
[blikk](https://github.com/Koromix/rygel/tree/master/src/blikk)   | Embeddable beginner-friendly programming language with static types, fast compilation                                                       | C++
[CNoke](https://www.npmjs.com/package/cnoke)   | Simple alternative to CMake.js, without any dependency, designed to build native Node addons based on CMake                                 | Javascript
[SeatSH](https://github.com/Koromix/rygel/tree/master/src/seatsh)  | SeatSH is a Windows service that enables you to launch graphical programs from an SSH connection on Windows 10, for automated test systems. | C++
[drd](https://github.com/Koromix/rygel/tree/master/src/drd)     | Alternative french PMSI MCO classifier<br>*Library (libdrd), command-line tool (drdc) and R package (drdR)*                                 | C++
[Luiggi](https://github.com/Koromix/luigi/tree/master/luiggi)   | Simple educational programming language implemented in JS                                                                                   | Javascript
[SaneBB](https://gist.github.com/Koromix/679c2e6336808f1e848f2e7f2e6c39cb)  | Small (single-file) and fast BBCode parser in Java                                                                                          | Java
[Meestic](https://github.com/Koromix/rygel/tree/master/src/meestic) | Small command-line utility to control MSI Delta 15 keyboard lighting (and other models?). Provided "as is", no guarantee.                   | C++
