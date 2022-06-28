# Benchmarks

## Overview

Here is a quick overview of the execution time of Koffi calls on three benchmarks, where it is compared to a theoretical ideal FFI implementation (approximated with pre-compiled static N-API glue code):

- The first benchmark is based on `rand()` calls
- The second benchmark is based on `atoi()` calls
- The third benchmark is based on [Raylib](https://www.raylib.com/)

<table style="margin: 0 auto;">
    <tr>
        <td><a href="_static/perf_linux_20220628.png" target="_blank"><img src="_static/perf_linux_20220628.png" alt="Linux performance" style="width: 350px;"/></a></td>
        <td><a href="_static/perf_windows_20220628.png" target="_blank"><img src="_static/perf_windows_20220628.png" alt="Windows performance" style="width: 350px;"/></a></td>
    </tr>
</table>

These results are detailed and explained below, and compared to node-ffi/node-ffi-napi.

## Linux x86_64

The results presented below were measured on my x86_64 Linux machine (Intel® Core™ i5-4460).

### rand results

This test is based around repeated calls to a simple standard C function atoi, and has three implementations:

- the first one is the reference, it calls atoi through an N-API module, and is close to the theoretical limit of a perfect (no overhead) Node.js > C FFI implementation (pre-compiled static glue code)
- the second one calls atoi through Koffi
- the third one uses the official Node.js FFI implementation, node-ffi-napi

Benchmark     | Iteration time | Relative performance | Overhead
------------- | -------------- | -------------------- | --------
rand_napi     | 644 ns         | x1.00                | (ref)
rand_koffi    | 950 ns         | x0.68                | +48%
rand_node_ffi | 30350 ns       | x0.02                | +4613%

Because rand is a pretty small function, the FFI overhead is clearly visible.

### atoi results

This test is similar to the rand one, but it is based on atoi, which takes a string parameter. Javascript (V8) to C string conversion is relatively slow and heavy.

Benchmark     | Iteration time | Relative performance | Overhead
------------- | -------------- | -------------------- | --------
atoi_napi     | 1104 ns        | x1.00                | (ref)
atoi_koffi    | 1778 ns        | x0.62                | +61%
atoi_node_ffi | 125300 ns      | x0.009               | +11250%

Because atoi is a pretty small function, the FFI overhead is clearly visible.

### Raylib results

This benchmark uses the CPU-based image drawing functions in Raylib. The calls are much heavier than in the atoi benchmark, thus the FFI overhead is reduced. In this implementation, Koffi is compared to:

- Baseline: Full C++ version of the code (no JS)
- [node-raylib](https://github.com/RobLoach/node-raylib): This is a native wrapper implemented with N-API

Benchmark          | Iteration time | Relative performance | Overhead
------------------ | -------------- | -------------------- | --------
raylib_cc          | 215.7 µs       | x1.20                | -17%
raylib_node_raylib | 258.9 µs       | x1.00                | (ref)
raylib_koffi       | 311.6 µs       | x0.83                | +20%
raylib_node_ffi    | 928.4 µs       | x0.28                | +259%

## Windows x86_64

The results presented below were measured on my x86_64 Windows machine (Intel® Core™ i5-4460).

### rand results

This test is based around repeated calls to a simple standard C function atoi, and has three implementations:

- the first one is the reference, it calls atoi through an N-API module, and is close to the theoretical limit of a perfect (no overhead) Node.js > C FFI implementation (pre-compiled static glue code)
- the second one calls atoi through Koffi
- the third one uses the official Node.js FFI implementation, node-ffi-napi

Benchmark     | Iteration time | Relative performance | Overhead
------------- | -------------- | -------------------- | --------
rand_napi     | 965 ns         | x1.00                | (ref)
rand_koffi    | 1248 ns        | x0.77                | +29%
rand_node_ffi | 41500 ns       | x0.02                | +4203%

Because rand is a pretty small function, the FFI overhead is clearly visible.

### atoi results

This test is similar to the rand one, but it is based on atoi, which takes a string parameter. Javascript (V8) to C string conversion is relatively slow and heavy.

The results below were measured on my x86_64 Windows machine (Intel® Core™ i5-4460):

Benchmark     | Iteration time | Relative performance | Overhead
------------- | -------------- | -------------------- | --------
atoi_napi     | 1393 ns        | x1.00                | (ref)
atoi_koffi    | 2246 ns        | x0.62                | +61%
atoi_node_ffi | 157550 ns      | x0.009               | +11210%

Because atoi is a pretty small function, the FFI overhead is clearly visible.

### Raylib results

This benchmark uses the CPU-based image drawing functions in Raylib. The calls are much heavier than in the atoi benchmark, thus the FFI overhead is reduced. In this implementation, Koffi is compared to:

- Baseline: Full C++ version of the code (no JS)
- [node-raylib](https://github.com/RobLoach/node-raylib): This is a native wrapper implemented with N-API

Benchmark          | Iteration time | Relative performance | Overhead
------------------ | -------------- | -------------------- | --------
raylib_cc          | 211.8 µs       | x1.25                | -20%
raylib_node_raylib | 264.4 µs       | x1.00                | (ref)
raylib_koffi       | 318.9 µs       | x0.83                | +21%
raylib_node_ffi    | 1146.2 µs      | x0.23                | +334%

Please note that in order to get fair numbers for raylib_node_raylib, it was recompiled with clang-cl before running the benchmark with the following commands:

```batch
cd node_modules\raylib
rmdir /S /Q bin build
npx cmake-js compile -t ClangCL
```

## Running benchmarks

Open a console, go to `koffi/benchmark` and run `../../cnoke/cnoke.js` (or `node ..\..\cnoke\cnoke.js` on Windows) before doing anything else.

```sh
cd koffi/benchmark
node ../../cnoke/cnoke.js
```

Please note that the *x86_64 Windows results above are measured with Clang*, which you can use too with the following commands:

```batch
cd koffi\benchmark
node ..\..\cnoke\cnoke.js --toolset ClangCL
```

Once everything is built and ready, run:

```sh
node benchmark.js
```
