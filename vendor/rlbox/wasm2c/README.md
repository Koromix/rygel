[![Tests](https://github.com/PLSysSec/rlbox_wasm2c_sandbox/actions/workflows/cmake.yml/badge.svg)](https://github.com/PLSysSec/rlbox_wasm2c_sandbox/actions/workflows/cmake.yml)

# RLBox Wasm2c Sandbox Integration

Integration with RLBox sandboxing API to leverage the sandboxing in WASM modules compiled with the wasm2c compiler (from the [wabt](https://github.com/WebAssembly/wabt/) toolsuite).

For details about the RLBox sandboxing APIs, see [here](https://github.com/PLSysSec/rlbox).

## Reporting security bugs

If you find a security bug, please do not create a public issue. Instead, file a security bug on bugzilla using the [following template link](https://bugzilla.mozilla.org/enter_bug.cgi?cc=tom%40mozilla.com&cc=nfroyd%40mozilla.com&cc=deian%40cs.ucsd.edu&cc=shravanrn%40gmail.com&component=Security%3A%20Process%20Sandboxing&defined_groups=1&groups=core-security&product=Core&bug_type=defect).

## Building/Running the tests

You can build and run the tests using cmake with the following commands.

```bash
cmake -S . -B ./build
cmake --build ./build --parallel
cmake --build ./build --target test
```

On Arch Linux you'll need to install [ncurses5-compat-libs](https://aur.archlinux.org/packages/ncurses5-compat-libs/).

## Using this tool

First, build the rlbox_wasm2c_sandbox repo with

```bash
cmake -S . -B ./build
cmake --build ./build --target all
```

This wasm2c/wasm integration with RLBox depends on 3 external tools/libraries that are pulled in **automatically** to run the tests included in this repo.

1. [A clang compiler with support for WASM/WASI backend, and the WASI sysroot](https://github.com/CraneStation/wasi-sdk). This allows you to compile C/C++ code to WASM modules usable outside of web browsers (in desktop applications).
2. [The wasm2c compiler](https://github.com/WebAssembly/wabt/) that compiles the produced WASM/WASI module to C code that you can compile with a standard C compiler.
3. [The RLBox APIs](https://github.com/PLSysSec/rlbox) - A set of APIs that allow easy use of sandboxed code. It handles ABI differences between sandboxed (Wasm) code and native code, and ensures that you include data sanitization checks on untrusted data returned by the sandboxed C code.

In the below steps, you can either use the automatically pulled in versions as described below, or download the tools yourself.

To sandbox a library of your choice and use the sandboxed library in an application follow the RLBox tutorial [here](https://rlbox.dev)


## Contributing Code

1. To contribute code, it is recommended you install clang-tidy which the build
uses if available. Install using:

   On Ubuntu:

   ```bash
   sudo apt install clang-tidy
   ```

   On Arch Linux:

   ```bash
   sudo pacman -S clang-tidy
   ```

2. It is recommended you use the dev mode for building during development. This
treat warnings as errors, enables clang-tidy checks, runs address sanitizer etc.
Also, you probably want to use the debug build. To do this, adjust your build
settings as shown below

   ```bash
   cmake -DCMAKE_BUILD_TYPE=Debug -DDEV=ON -S . -B ./build
   ```

3. After making changes to the source, add any new required tests and run all
tests as described earlier.

4. To make sure all code/docs are formatted with, we use clang-format.
Install using:

   On Ubuntu:

   ```bash
   sudo apt install clang-format
   ```

   On Arch Linux:

   ```bash
   sudo pacman -S clang-format
   ```

5. Format code with the format-source target:

   ```bash
   cmake --build ./build --target format-source
   ```

6. Submit the pull request.
