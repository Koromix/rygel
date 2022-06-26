# Benchmarks

## Overview

Here is a quick overview of the execution time of Koffi calls on three benchmarks, where it is compared to a theoretical ideal FFI implementation (approximated with pre-compiled static N-API glue code):

- The first benchmark is based on `rand()` calls
- The second benchmark is based on `atoi()` calls
- The third benchmark is based on [Raylib](https://www.raylib.com/)

<table style="margin: 0 auto;">
    <tr>
        <td><a href="_static/perf_linux_20220627.png" target="_blank"><img src="_static/perf_linux_20220627.png" alt="Linux performance" style="width: 350px;"/></a></td>
        <td><a href="_static/perf_windows_20220627.png" target="_blank"><img src="_static/perf_windows_20220627.png" alt="Windows performance" style="width: 350px;"/></a></td>
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

Benchmark     | Iterations | Total time  | Relative performance | Overhead
------------- | ---------- | ----------- | -------------------- | ----------
rand_napi     | 20000000   | 1.29s       | (baseline)           | (baseline)
rand_koffi    | 20000000   | 1.91s       | x0.68                | +48%
rand_node_ffi | 20000000   | 71.59s      | x0.02                | +5400%

Because rand is a pretty small function, the FFI overhead is clearly visible.

### atoi results

This test is similar to the rand one, but it is based on atoi, which takes a string parameter. Javascript (V8) to C string conversion is relatively slow and heavy.

Benchmark     | Iterations | Total time  | Relative performance | Overhead
------------- | ---------- | ----------- | -------------------- | ----------
atoi_napi     | 20000000   | 2.25s       | (baseline)           | (baseline)
atoi_koffi    | 20000000   | 3.40s       | x0.66                | +51%
atoi_node_ffi | 20000000   | 622.55s     | x0.004               | +27500%

Because atoi is a pretty small function, the FFI overhead is clearly visible.

### Raylib results

This benchmark uses the CPU-based image drawing functions in Raylib. The calls are much heavier than in the atoi benchmark, thus the FFI overhead is reduced. In this implementation, Koffi is compared to:

- Baseline: Full C++ version of the code (no JS)
- [node-raylib](https://github.com/RobLoach/node-raylib): This is a native wrapper implemented with N-API

Benchmark          | Iterations | Total time  | Relative performance | Overhead
---------------    | ---------- | ----------- | -------------------- | ----------
raylib_cc          | 100        | 7.79s       | x1.18                | -16%
raylib_node_raylib | 100        | 9.26s       | (baseline)           | (baseline)
raylib_koffi       | 100        | 11.18s      | x0.83                | +21%
raylib_node_ffi    | 100        | 34.91s      | x0.27                | +276%

## Windows x86_64

The results presented below were measured on my x86_64 Windows machine (Intel® Core™ i5-4460).

### rand results

This test is based around repeated calls to a simple standard C function atoi, and has three implementations:

- the first one is the reference, it calls atoi through an N-API module, and is close to the theoretical limit of a perfect (no overhead) Node.js > C FFI implementation (pre-compiled static glue code)
- the second one calls atoi through Koffi
- the third one uses the official Node.js FFI implementation, node-ffi-napi

Benchmark     | Iterations | Total time  | Relative performance | Overhead
------------- | ---------- | ----------- | -------------------- | ----------
rand_napi     | 20000000   | 1.98s       | (baseline)           | (baseline)
rand_koffi    | 20000000   | 2.60s       | x0.76                | +31%
rand_node_ffi | 20000000   | 87.13s      | x0.02                | +4300%

Because rand is a pretty small function, the FFI overhead is clearly visible.

### atoi results

This test is similar to the rand one, but it is based on atoi, which takes a string parameter. Javascript (V8) to C string conversion is relatively slow and heavy.

The results below were measured on my x86_64 Windows machine (Intel® Core™ i5-4460):

Benchmark     | Iterations | Total time  | Relative performance | Overhead
------------- | ---------- | ----------- | -------------------- | ----------
atoi_napi     | 20000000   | 2.80s       | (baseline)           | (baseline)
atoi_koffi    | 20000000   | 4.81s       | x0.58                | +72%
atoi_node_ffi | 20000000   | 467.24s     | x0.006               | +16600%

Because atoi is a pretty small function, the FFI overhead is clearly visible.

### Raylib results

This benchmark uses the CPU-based image drawing functions in Raylib. The calls are much heavier than in the atoi benchmark, thus the FFI overhead is reduced. In this implementation, Koffi is compared to:

- Baseline: Full C++ version of the code (no JS)
- [node-raylib](https://github.com/RobLoach/node-raylib): This is a native wrapper implemented with N-API

Benchmark          | Iterations | Total time  | Relative performance | Overhead
---------------    | ---------- | ----------- | -------------------- | ----------
raylib_cc          | 100        | 7.61s       | x1.25                | -20%
raylib_node_raylib | 100        | 9.61s       | (baseline)           | (baseline)
raylib_koffi       | 100        | 11.65s      | x0.82                | +21%
raylib_node_ffi    | 100        | 42.02s      | x0.23                | +337%

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

Once this is done, you can execute each implementation, e.g. `build/raylib_cc` or `node ./atoi_koffi.js`. You can optionally define a custom number of iterations, e.g. `node ./atoi_koffi.js 10000000`.

```sh
node ./atoi_napi.js
node ./atoi_koffi.js
```
