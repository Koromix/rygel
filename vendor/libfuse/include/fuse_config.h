#pragma once

#define PACKAGE_VERSION "3.18.0"

#if defined(__linux__)
    #define HAVE_CLOSE_RANGE
    #define HAVE_COPY_FILE_RANGE
    #define HAVE_FALLOCATE
    #define HAVE_FDATASYNC
    #define HAVE_FORK
    #define HAVE_FSTATAT
    // #define HAVE_ICONV
    #define HAVE_OPENAT
    #define HAVE_PIPE2
    #define HAVE_POSIX_FALLOCATE
    #define HAVE_READLINKAT
    #define HAVE_SETXATTR
    #define HAVE_SPLICE
    #define HAVE_STRUCT_STAT_ST_ATIM
    // #define HAVE_STRUCT_STAT_ST_ATIMESPEC
    #define HAVE_UTIMENSAT
    #define HAVE_VMSPLICE
#elif defined(__FreeBSD__)
    // #define HAVE_CLOSE_RANGE
    #define HAVE_COPY_FILE_RANGE
    // #define HAVE_FALLOCATE
    #define HAVE_FDATASYNC
    #define HAVE_FORK
    #define HAVE_FSTATAT
    // #define HAVE_ICONV
    #define HAVE_OPENAT
    #define HAVE_PIPE2
    #define HAVE_POSIX_FALLOCATE
    #define HAVE_READLINKAT
    // #define HAVE_SETXATTR
    // #define HAVE_SPLICE
    #define HAVE_STRUCT_STAT_ST_ATIM
    #define HAVE_STRUCT_STAT_ST_ATIMESPEC
    #define HAVE_UTIMENSAT
    // #define HAVE_VMSPLICE
#elif defined(__OpenBSD__)
    // #define HAVE_CLOSE_RANGE
    // #define HAVE_COPY_FILE_RANGE
    // #define HAVE_FALLOCATE
    #define HAVE_FDATASYNC
    #define HAVE_FORK
    #define HAVE_FSTATAT
    // #define HAVE_ICONV
    #define HAVE_OPENAT
    #define HAVE_PIPE2
    // #define HAVE_POSIX_FALLOCATE
    #define HAVE_READLINKAT
    // #define HAVE_SETXATTR
    // #define HAVE_SPLICE
    #define HAVE_STRUCT_STAT_ST_ATIM
    #define HAVE_STRUCT_STAT_ST_ATIMESPEC
    #define HAVE_UTIMENSAT
    // #define HAVE_VMSPLICE
#endif
