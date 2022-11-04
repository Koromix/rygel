<!-- Title: koromix.dev — Misc
     Menu: Other
     Created: 2022-05-16 -->

#blikk# blikk

blikk is an **embeddable beginner-friendly programming language** with static types, fast compilation.

You can find out more [in the code repository](https://github.com/Koromix/rygel/tree/master/src/blikk).

#cnoke# CNoke

CNoke is a **Javascript NPM package designed to build native Node addons based on CMake**, without any dependency. It can be used as an alternative to CMake.js.

Install it like this:

    npm install cnoke

It obviously requires [CMake](http://www.cmake.org/download/) and a proper C/C++ toolchain:

- *Windows*: Visual C++ Build Tools or a recent version of Visual C++ will do (the free Community version works well)
- *POSIX (Linux, macOS, etc.)*: Clang or GCC, and Make (Ninja is preferred if available)

Once everything is in place, get started with your addon with the CMakeLists.txt template:

    cmake_minimum_required(VERSION 3.11)
    project(hello C CXX)

    find_package(CNoke)

    add_node_addon(NAME hello SOURCES hello.cc)

You can find out more [in the NPM repository](https://www.npmjs.com/package/cnoke).

#seatsh# SeatSH

SeatSH is a Windows service that enables you to **launch graphical applications from an SSH connection on Windows 10**. As an example, try to start `notepad` from an SSH connection: it won't work, because this session does not have access to the active console (desktop) session.

SeatSH runs as a service, waits for a client to connect, and launches the command line specified by the client on the active console session. Once the service is installed, launching a graphical application from the command line is as simple as `seatsh notepad`.

It will transparently forward standard streams (stdin, stdout, stderr) and report the application exit code as its own.

It is mainly useful for automated test systems, do not install it on production machines because it is a security risk. SeatSH only makes sure that the user connected to the SSH matches the one on the active console session, nothing more.

You can find out more [in the code repository](https://github.com/Koromix/rygel/tree/master/src/seatsh).

#drd# drd

The drd project strives to provide a **fully-featured and up-to-date alternative french PMSI MCO classifier**.

Using the data files (.tab) provided with GenRSA, it can classify MCO stays from 2013 to 2022 *several hundred times faster* than the official classifier (on a 4 core computer from 2015, it can classify around 4 million stays per second).

It can be used in three ways:

- *libdrd*: C++ library, usable as a single-header file.
- *drdc*: command-line tool.
- *drdR*: R package (load stays as data.frames, and classify them).

The classifier was carefully assembled from the documentation and through retro-engineering of the official classifier (GenRSA), without source code access.

You can find out more [in the code repository](https://github.com/Koromix/rygel/tree/master/src/drd).

#sanebb# SaneBB

SaneBB is a **small and fast BBCode parser in Java**, which I made as an alternative to the horribly overengineered and [inner-platform effect](https://en.wikipedia.org/wiki/Inner-platform_effect) victim [KefirBB library](https://github.com/kefirfromperm/kefirbb).

It's a single Java file, the documentation is included at the top of the file.

You can find it here [on GitHub gist](https://gist.github.com/Koromix/679c2e6336808f1e848f2e7f2e6c39cb).

#meestic# Meestic

Meestic is a small command-line tool to **control the keyboard lighting on MSI Delta 15 laptops**, which may or may not support any other device.

You can find out more [in the code repository](https://github.com/Koromix/rygel/tree/master/src/meestic).
