# Overview

Here is a quick overview of the execution time of Koffi calls on three benchmarks, where it is compared to a theoretical ideal FFI implementation (approximated with pre-compiled static N-API glue code):

- The first benchmark is based on `rand()` calls
- The second benchmark is based on `atoi()` calls
- The third benchmark is based on [Raylib](https://www.raylib.com/)

<p style="text-align: center;">
    <a href="{{ ASSET static/perf_linux.png }}" target="_blank"><img src="{{ ASSET static/perf_linux.png }}" alt="Linux x86_64 performance" style="width: 350px;"/></a>
    <a href="{{ ASSET static/perf_windows.png }}" target="_blank"><img src="{{ ASSET static/perf_windows.png }}" alt="Windows x86_64 performance" style="width: 350px;"/></a>
</p>

These results are detailed and explained below, and compared to node-ffi/node-ffi-napi.

# Linux x86_64

The results presented below were measured on my x86_64 Linux machine (Intel® Core™ i5-4460).

## rand results

This test is based around repeated calls to a simple standard C function `rand`, and has three implementations:

- the first one is the reference, it calls rand through an N-API module, and is close to the theoretical limit of a perfect (no overhead) Node.js > C FFI implementation (pre-compiled static glue code)
- the second one calls rand through Koffi
- the third one uses the official Node.js FFI implementation, node-ffi-napi

Benchmark     | Iteration time | Relative performance | Overhead
------------- | -------------- | -------------------- | --------
rand_napi     | 700 ns         | x1.00                | (ref)
rand_koffi    | 1152 ns        | x0.61                | +64%
rand_node_ffi | 32750 ns       | x0.02                | +4576%

Because rand is a pretty small function, the FFI overhead is clearly visible.

## atoi results

This test is similar to the rand one, but it is based on `atoi`, which takes a string parameter. Javascript (V8) to C string conversion is relatively slow and heavy.

Benchmark     | Iteration time | Relative performance | Overhead
------------- | -------------- | -------------------- | --------
atoi_napi     | 1028 ns        | x1.00                | (ref)
atoi_koffi    | 1730 ns        | x0.59                | +68%
atoi_node_ffi | 121670 ns      | x0.008               | +11738%

Because atoi is a pretty small function, the FFI overhead is clearly visible.

## Raylib results

This benchmark uses the CPU-based image drawing functions in Raylib. The calls are much heavier than in previous benchmarks, thus the FFI overhead is reduced. In this implementation, Koffi is compared to:

- Baseline: Full C++ version of the code (no JS)
- [node-raylib](https://github.com/RobLoach/node-raylib): This is a native wrapper implemented with N-API

Benchmark          | Iteration time | Relative performance | Overhead
------------------ | -------------- | -------------------- | --------
raylib_cc          | 18.5 µs        | x1.42                | -30%
raylib_node_raylib | 26.3 µs        | x1.00                | (ref)
raylib_koffi       | 28.0 µs        | x0.94                | +6%
raylib_node_ffi    | 87.0 µs        | x0.30                | +230%

# Windows x86_64

The results presented below were measured on my x86_64 Windows machine (Intel® Core™ i5-4460).

## rand results

This test is based around repeated calls to a simple standard C function `rand`, and has three implementations:

- the first one is the reference, it calls rand through an N-API module, and is close to the theoretical limit of a perfect (no overhead) Node.js > C FFI implementation (pre-compiled static glue code)
- the second one calls rand through Koffi
- the third one uses the official Node.js FFI implementation, node-ffi-napi

Benchmark     | Iteration time | Relative performance | Overhead
------------- | -------------- | -------------------- | --------
rand_napi     | 859 ns         | x1.00                | (ref)
rand_koffi    | 1352 ns        | x0.64                | +57%
rand_node_ffi | 35640 ns       | x0.02                | +4048%

Because rand is a pretty small function, the FFI overhead is clearly visible.

## atoi results

This test is similar to the rand one, but it is based on `atoi`, which takes a string parameter. Javascript (V8) to C string conversion is relatively slow and heavy.

The results below were measured on my x86_64 Windows machine (Intel® Core™ i5-4460):

Benchmark     | Iteration time | Relative performance | Overhead
------------- | -------------- | -------------------- | --------
atoi_napi     | 1336 ns        | x1.00                | (ref)
atoi_koffi    | 2440 ns        | x0.55                | +83%
atoi_node_ffi | 136890 ns      | x0.010               | +10144%

Because atoi is a pretty small function, the FFI overhead is clearly visible.

## Raylib results

This benchmark uses the CPU-based image drawing functions in Raylib. The calls are much heavier than in the atoi benchmark, thus the FFI overhead is reduced. In this implementation, Koffi is compared to:

- [node-raylib](https://github.com/RobLoach/node-raylib) (baseline): This is a native wrapper implemented with N-API
- raylib_cc: C++ implementation of the benchmark, without any Javascript

Benchmark          | Iteration time | Relative performance | Overhead
------------------ | -------------- | -------------------- | --------
raylib_cc          | 18.2 µs        | x1.50                | -33%
raylib_node_raylib | 27.3 µs        | x1.00                | (ref)
raylib_koffi       | 29.8 µs        | x0.92                | +9%
raylib_node_ffi    | 96.3 µs        | x0.28                | +253%

# Running benchmarks

Please note that all benchmark results on this page are made with Clang-built binaries.

```sh
cd koffi
node ../../cnoke/cnoke.js --prefer-clang

cd koffi/benchmark
node ../../cnoke/cnoke.js --prefer-clang
```

Once everything is built and ready, run:

```sh
node benchmark.js
```
