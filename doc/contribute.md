# Contributing

## Bugs and feature requests

Use the official repository (named Luigi, because this is a monorepo containing multiple projects) for bugs, ideas and features requests.

Go here: https://github.com/Koromix/luigi/issues

## Code style

Koffi is programmed in a mix of C++ and assembly code (architecture-specific code). It uses [node-addon-api](https://github.com/nodejs/node-addon-api) (C++ N-API wrapper) to interact with Node.js.

My personal preference goes to a rather C-like C++ style, with careful use of templates (mainly for containers) and little object-oriented programming. I strongly prefer tagged unions and code locality over inheritance and virtual methods. Exceptions are disabled.

## Build from source

We provide prebuilt binaries, packaged in the NPM archive, so in most cases it should be as simple as `npm install koffi`. If you want to hack Koffi or use a specific platform, follow the instructions below.

Start by cloning the repository with [Git](https://git-scm.com/):

```sh
git clone https://github.com/Koromix/luigi
cd luigi/koffi
```

As said before, this is a monorepository containg multiple projects, hence the name.

### Windows

First, make sure the following dependencies are met:

- The "Desktop development with C++" workload from [Visual Studio 2022 or 2019](https://visualstudio.microsoft.com/downloads/) or the "C++ build tools" workload from the [Build Tools](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022), with the default optional components.
- [CMake meta build system](https://cmake.org/)
- [Node.js](https://nodejs.org/) 12 or later

Once this is done, run this command _from the test or the benchmark directory_ (depending on what you want to build):

```sh
cd koffi/test # or cd koffi/benchmark
node ../../cnoke/cnoke.js
```

### Other platforms

Make sure the following dependencies are met:

- `gcc` and `g++` >= 8.3 or newer
- GNU Make 3.81 or newer
- [CMake meta build system](https://cmake.org/)
- [Node.js](https://nodejs.org/) 12 or later

Once this is done, run this command _from the test or the benchmark directory_ (depending on what you want to build):

```sh
cd koffi/test # or cd koffi/benchmark
node ../../cnoke/cnoke.js
```

## Running tests

Koffi is tested on multiple architectures using emulated (accelerated when possible) QEMU machines. First, you need to install qemu packages, such as `qemu-system` (or even `qemu-system-gui`) on Ubuntu.

These machines are not included directly in this repository (for license and size reasons), but they are available here: https://koromix.dev/files/machines/

For example, if you want to run the tests on Debian ARM64, run the following commands:

```sh
cd luigi/koffi/qemu/
wget -q -O- https://koromix.dev/files/machines/qemu_debian_arm64.tar.zst | zstd -d | tar xv
sha256sum -c --ignore-missing registry/sha256sum.txt
```

Note that the machine disk content may change each time the machine runs, so the checksum test will fail once a machine has been used at least once.

And now you can run the tests with:

```sh
node qemu.js # Several options are available, use --help
```

And be patient, this can be pretty slow for emulated machines. The Linux machines have and use ccache to build Koffi, so subsequent build steps will get much more tolerable.

By default, machines are started and stopped for each test. But you can start the machines ahead of time and run the tests multiple times instead:

```sh
node qemu.js start # Start the machines
node qemu.js # Test (without shutting down)
node qemu.js # Test again
node qemu.js stop # Stop everything
```

You can also restrict the test to a subset of machines:

```sh
# Full test cycle
node qemu.js test debian_x64 debian_i386

# Separate start, test, shutdown
node qemu.js start debian_x64 debian_i386
node qemu.js test debian_x64 debian_i386
node qemu.js stop
```

Finally, you can join a running machine with SSH with the following shortcut, if you need to do some debugging or any other manual procedure:

```sh
node qemu.js ssh debian_i386
```

Each machine is configured to run a VNC server available locally, which you can use to access the display, using KRDC or any other compatible viewer. Use the `info` command to get the VNC port.

```sh
node qemu.js info debian_x64
```

## Todo list

After the release of version 1.3.0, the current priorities for the next major release are:

- Automate Windows/AArch64 (qemu) and macOS/AArch64 (how?) tests and builds
- Create a real-world example, using several libraries (Raylib, SQLite, libsodium) to illustrate how to work with various C API styles

The following features are also planned eventually, not necessarily in that order:

- Optimize passing of structs and arrays, with separate HFA-specific helper functions
- Add simple struct type parser
- Add more ways to manually encode and decode various types to and from byte arrays
- Add support for unions
- Provide better ways to automatically deal with caller/heap-allocated memory (strings, etc.)
- Port Koffi to PowerPC (POWER9+) ABI
- Fix assembly unwind and CFI directives for better debugging experience
