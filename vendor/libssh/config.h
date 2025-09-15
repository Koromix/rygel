#define PACKAGE "libssh"
#define VERSION "0.11.3"

#if defined(_WIN32)
    #define GLOBAL_BIND_CONFIG "/etc/ssh/libssh_server_config"
    #define GLOBAL_CLIENT_CONFIG "/etc/ssh/ssh_config"

    #define HAVE_SYS_UTIME_H 1
    #define HAVE_IO_H 1
    #define HAVE_STDINT_H 1
    #define HAVE_WSPIAPI_H 1
    #define HAVE_ECC 1

    #define HAVE_SNPRINTF 1
    #define HAVE__SNPRINTF 1
    #define HAVE__SNPRINTF_S 1
    #define HAVE_VSNPRINTF 1
    #define HAVE__VSNPRINTF 1
    #define HAVE__VSNPRINTF_S 1
    #define HAVE_ISBLANK 1
    #define HAVE_STRNCPY 1
    #define HAVE_GETADDRINFO 1
    #define HAVE_SELECT 1
    // #define HAVE_NTOHLL 1
    // #define HAVE_HTONLL 1
    #define HAVE_STRTOULL 1
    #define HAVE__STRTOUI64 1
    #define HAVE_SECURE_ZERO_MEMORY 1

    #define HAVE_MSC_THREAD_LOCAL_STORAGE 1
    #define HAVE_COMPILER__FUNC__ 1
    #define HAVE_COMPILER__FUNCTION__ 1

    #define WITH_ZLIB 1
    #define WITH_SFTP 1
    #define WITH_GEX 1
#elif defined(__APPLE__)
    #define GLOBAL_BIND_CONFIG "/etc/ssh/libssh_server_config"
    #define GLOBAL_CLIENT_CONFIG "/etc/ssh/ssh_config"

    #define HAVE_ARPA_INET_H 1
    #define HAVE_GLOB_H 1
    #define HAVE_UTMP_H 1
    #define HAVE_UTIL_H 1
    #define HAVE_SYS_TIME_H 1
    #define HAVE_TERMIOS_H 1
    #define HAVE_UNISTD_H 1
    #define HAVE_STDINT_H 1
    #define HAVE_PTHREAD_H 1
    #define HAVE_ECC 1
    #define HAVE_GLOB_GL_FLAGS_MEMBER 1

    #define HAVE_SNPRINTF 1
    #define HAVE_VSNPRINTF 1
    #define HAVE_ISBLANK 1
    #define HAVE_STRNCPY 1
    #define HAVE_STRNDUP 1
    #define HAVE_CFMAKERAW 1
    #define HAVE_GETADDRINFO 1
    #define HAVE_POLL 1
    #define HAVE_SELECT 1
    #define HAVE_NTOHLL 1
    #define HAVE_HTONLL 1
    #define HAVE_STRTOULL 1
    #define HAVE_GLOB 1
    #define HAVE_MEMSET_S 1

    #define HAVE_PTHREAD 1
    #define HAVE_GCC_THREAD_LOCAL_STORAGE 1
    #define HAVE_FALLTHROUGH_ATTRIBUTE 1
    #define HAVE_UNUSED_ATTRIBUTE 1
    #define HAVE_WEAK_ATTRIBUTE 1
    // #define HAVE_CONSTRUCTOR_ATTRIBUTE 1
    // #define HAVE_DESTRUCTOR_ATTRIBUTE 1
    #define HAVE_GCC_VOLATILE_MEMORY_PROTECTION 1
    #define HAVE_COMPILER__FUNC__ 1
    #define HAVE_COMPILER__FUNCTION__ 1

    #define WITH_ZLIB 1
    #define WITH_SFTP 1
    #define WITH_GEX 1
#elif defined(__EMSCRIPTEN__)
    #define GLOBAL_BIND_CONFIG "/etc/ssh/libssh_server_config"
    #define GLOBAL_CLIENT_CONFIG "/etc/ssh/ssh_config"

    #define HAVE_ARPA_INET_H 1
    #define HAVE_GLOB_H 1
    #define HAVE_PTY_H 1
    #define HAVE_UTMP_H 1
    #define HAVE_SYS_TIME_H 1
    #define HAVE_TERMIOS_H 1
    #define HAVE_UNISTD_H 1
    #define HAVE_STDINT_H 1
    #define HAVE_PTHREAD_H 1

    #define HAVE_SNPRINTF 1
    #define HAVE_VSNPRINTF 1
    #define HAVE_ISBLANK 1
    #define HAVE_STRNCPY 1
    #define HAVE_STRNDUP 1
    #define HAVE_CFMAKERAW 1
    #define HAVE_GETADDRINFO 1
    #define HAVE_POLL 1
    #define HAVE_SELECT 1
    #define HAVE_CLOCK_GETTIME 1
    #define HAVE_STRTOULL 1
    #define HAVE_GLOB 1
    #define HAVE_EXPLICIT_BZERO 1

    // #define HAVE_PTHREAD 1

    #define HAVE_GCC_THREAD_LOCAL_STORAGE 1
    #define HAVE_FALLTHROUGH_ATTRIBUTE 1
    #define HAVE_UNUSED_ATTRIBUTE 1
    #define HAVE_WEAK_ATTRIBUTE 1
    #define HAVE_CONSTRUCTOR_ATTRIBUTE 1
    #define HAVE_DESTRUCTOR_ATTRIBUTE 1
    #define HAVE_GCC_VOLATILE_MEMORY_PROTECTION 1
    #define HAVE_COMPILER__FUNC__ 1
    #define HAVE_COMPILER__FUNCTION__ 1

    #define WITH_ZLIB 1
    #define WITH_SFTP 1
    #define WITH_GEX 1
#else
    #define GLOBAL_BIND_CONFIG "/etc/ssh/libssh_server_config"
    #define GLOBAL_CLIENT_CONFIG "/etc/ssh/ssh_config"

    #define HAVE_ARPA_INET_H 1
    #define HAVE_GLOB_H 1
    #define HAVE_SYS_TIME_H 1
    #define HAVE_TERMIOS_H 1
    #define HAVE_UNISTD_H 1
    #define HAVE_STDINT_H 1
    #define HAVE_PTHREAD_H 1
    #define HAVE_ECC 1
    #define HAVE_GLOB_GL_FLAGS_MEMBER 1

    #define HAVE_SNPRINTF 1
    #define HAVE_VSNPRINTF 1
    #define HAVE_ISBLANK 1
    #define HAVE_STRNCPY 1
    #define HAVE_STRNDUP 1
    #define HAVE_CFMAKERAW 1
    #define HAVE_GETADDRINFO 1
    #define HAVE_POLL 1
    #define HAVE_SELECT 1
    #define HAVE_CLOCK_GETTIME 1
    #define HAVE_STRTOULL 1
    #define HAVE_GLOB 1
    #define HAVE_EXPLICIT_BZERO 1
    #if defined(__FreeBSD__) || defined(__OpenBSD__)
        #define HAVE_MEMSET_S 1
    #endif

    #define HAVE_PTHREAD 1

    #define HAVE_GCC_THREAD_LOCAL_STORAGE 1
    #define HAVE_FALLTHROUGH_ATTRIBUTE 1
    #define HAVE_UNUSED_ATTRIBUTE 1
    #define HAVE_WEAK_ATTRIBUTE 1
    // #define HAVE_CONSTRUCTOR_ATTRIBUTE 1
    // #define HAVE_DESTRUCTOR_ATTRIBUTE 1
    #define HAVE_GCC_VOLATILE_MEMORY_PROTECTION 1
    #define HAVE_COMPILER__FUNC__ 1
    #define HAVE_COMPILER__FUNCTION__ 1

    #define WITH_ZLIB 1
    #define WITH_SFTP 1
    #define WITH_GEX 1
#endif

#ifndef NDEBUG
    #define DEBUG_CALLTRACE 1
#endif
