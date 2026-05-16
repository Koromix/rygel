# Overview

Here is a quick overview of the execution time of Koffi calls on three benchmarks, where it is compared to a theoretical ideal FFI implementation (approximated with pre-compiled static N-API glue code):

- The first benchmark is based on `rand()` calls
- The second benchmark is based on `atoi()` calls
- The third benchmark is based on `memset()` calls

<div class="benchmark chart" data-platform="linux_x64"></div>
<div class="benchmark chart" data-platform="darwin_arm64"></div>
<div class="benchmark chart" data-platform="win32_x64"></div>

These results are detailed and explained below, and compared to node-ffi/node-ffi-napi.

# Linux x86_64

The results presented below were measured on my x86_64 Linux machine (Intel® Core™ Ultra 9 185H).

## rand results

This test is based around repeated calls to a simple standard C function `rand`, which takes no parameter and returns a 32-bit integer.

<div class="benchmark table" data-platform="linux_x64" data-benchmark="rand"></div>

Because rand is a pretty small function, the FFI overhead is clearly visible.

## atoi results

This test is similar to the rand one, but it is based on `atoi`, which takes a string parameter. Javascript (V8) to C string conversion is relatively slow and heavy.

<div class="benchmark table" data-platform="linux_x64" data-benchmark="atoi"></div>

## memset results

This test is based around repeated calls to the standard C function `memset`. All implementations pass a Node.js Buffer for the pointer argument.

<div class="benchmark table" data-platform="linux_x64" data-benchmark="memset"></div>

# macOS ARM64

The results presented below were measured on an Apple Mac mini M2 hosted by Scaleway.

## rand results

This test is based around repeated calls to a simple standard C function `rand`, which takes no parameter and returns a 32-bit integer.

<div class="benchmark table" data-platform="darwin_arm64" data-benchmark="rand"></div>

Because rand is a pretty small function, the FFI overhead is clearly visible.

## atoi results

This test is similar to the rand one, but it is based on `atoi`, which takes a string parameter. Javascript (V8) to C string conversion is relatively slow and heavy.

<div class="benchmark table" data-platform="darwin_arm64" data-benchmark="atoi"></div>

## memset results

This test is based around repeated calls to the standard C function `memset`. All implementations pass a Node.js Buffer for the pointer argument.

<div class="benchmark table" data-platform="darwin_arm64" data-benchmark="memset"></div>

# Windows x86_64

The results presented below were measured on my x86_64 Windows machine (Intel® Core™ i5-4460).

## rand results

This test is based around repeated calls to a simple standard C function `rand`, which takes no parameter and returns a 32-bit integer.

<div class="benchmark table" data-platform="win32_x64" data-benchmark="rand"></div>

## atoi results

This test is similar to the rand one, but it is based on `atoi`, which takes a string parameter. Javascript (V8) to C string conversion is relatively slow and heavy.

<div class="benchmark table" data-platform="win32_x64" data-benchmark="atoi"></div>

## memset results

This test is based around repeated calls to the standard C function `memset`. All implementations pass a Node.js Buffer for the pointer argument.

<div class="benchmark table" data-platform="win32_x64" data-benchmark="memset"></div>

# Running benchmarks

Please note that all benchmark results on this page are made with Clang-built binaries.

```sh
cd src/koffi
node ../cnoke/cnoke.js --clang --release

cd benchmark
node ../../cnoke/cnoke.js --clang --release
```

Once everything is built and ready, run:

```sh
node benchmark.js
```

<script src="{{ ASSET static/benchmarks.js }}"></script>
