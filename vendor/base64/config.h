#pragma once

#if defined(__x86_64__) || defined(_M_AMD64)
    #define HAVE_SSSE3 1
    #define HAVE_SSE41 1
    #define HAVE_SSE42 1
    #define HAVE_AVX 1
    #define HAVE_AVX2 1
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define HAVE_NEON64 1
#endif
