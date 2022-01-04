#define PACKAGE "libmicrohttpd"
#define PACKAGE_BUGREPORT "libmicrohttpd@gnu.org"
#define PACKAGE_NAME "GNU Libmicrohttpd"
#define PACKAGE_STRING "GNU Libmicrohttpd 0.9.75"
#define PACKAGE_TARNAME "libmicrohttpd"
#define PACKAGE_URL "http://www.gnu.org/software/libmicrohttpd/"
#define PACKAGE_VERSION "0.9.75"
#define VERSION "0.9.75"

#if defined(__MINGW32__)
    #define BAUTH_SUPPORT 1
    #define DAUTH_SUPPORT 1
    #define HAVE_ASSERT 1
    #define HAVE_C_VARARRAYS 1
    #define HAVE_CALLOC 1
    #define HAVE_CLOCK_GETTIME 1
    #define HAVE_ERRNO_H 1
    #define HAVE_FCNTL_H 1
    #define HAVE_FSEEKO 1
    #define HAVE_GETTIMEOFDAY 1
    #define HAVE_INET6 1
    #define HAVE_INTTYPES_H 1
    #define HAVE_LIMITS_H 1
    #define HAVE_LSEEK64 1
    #define HAVE_MEMORY_H 1
    #define HAVE_MESSAGES 1
    #define HAVE_NANOSLEEP 1
    #define HAVE_POSTPROCESSOR 1
    #define HAVE_PTHREAD_H 1
    #define HAVE_PTHREAD_PRIO_INHERIT 1
    #define HAVE_RAND 1
    #define HAVE_REAL_BOOL 1
    #define HAVE_SDKDDKVER_H 1
    #define HAVE_SEARCH_H 1
    #define HAVE_SNPRINTF 1
    #define HAVE_STDBOOL_H 1
    #define HAVE_STDDEF_H 1
    #define HAVE_STDINT_H 1
    #define HAVE_STDIO_H 1
    #define HAVE_STDLIB_H 1
    #define HAVE_STRINGS_H 1
    #define HAVE_STRING_H 1
    #define HAVE_SYS_PARAM_H 1
    #define HAVE_SYS_STAT_H 1
    #define HAVE_SYS_TIME_H 1
    #define HAVE_SYS_TYPES_H 1
    #define HAVE_TIME_H 1
    #define HAVE_TSEARCH 1
    #define HAVE_UNISTD_H 1
    #define HAVE_USLEEP 1
    #define HAVE_W32_GMTIME_S 1
    #define HAVE_WINDOWS_H 1
    #define HAVE_WINSOCK2_H 1
    #define HAVE_WS2TCPIP_H 1
    #define HAVE___FUNCTION__ 1
    #define HAVE___FUNC__ 1
    #define INLINE_FUNC 1
    #define LT_OBJDIR ".libs/"
    #define MHD_HAVE___BUILTIN_BSWAP32 1
    #define MHD_HAVE___BUILTIN_BSWAP64 1
    #define MHD_NO_THREAD_NAMES 1
    #define MHD_USE_W32_THREADS 1
    #define MINGW 1
    #define STDC_HEADERS 1
    #define UPGRADE_SUPPORT 1
    #define WINDOWS 1
    #define _FILE_OFFSET_BITS 64
    #define _GNU_SOURCE 1
    #define _MHD_EXTERN __attribute__((visibility("default"))) __declspec(dllexport) extern
    #define _MHD_ITC_SOCKETPAIR 1
    #define _MHD_static_inline static inline __attribute__((always_inline))
    #define _MHD_NORETURN __attribute__((noreturn))
    #define _XOPEN_SOURCE 1
    #define _XOPEN_SOURCE_EXTENDED 1
    #define FUNC_ATTR_NOSANITIZE_WORKS 1
    #define FUNC_ATTR_PTRCOMPARE_WOKRS 1
    #if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__)
        #define SIZEOF_SIZE_T 8
    #else
        #define SIZEOF_SIZE_T 4
    #endif
    #ifdef _USE_32BIT_TIME_T
        #define SIZEOF_STRUCT_TIMEVAL_TV_SEC 4
    #else
        #define SIZEOF_STRUCT_TIMEVAL_TV_SEC 8
    #endif
    #define SIZEOF_UINT64_T 8
    #define SIZEOF_UNSIGNED_INT 4
    #define SIZEOF_UNSIGNED_LONG_LONG 8
#elif defined(_MSC_VER)
    #define WINDOWS 1
    #define MSVC 1
    #define INLINE_FUNC 1
    #define BAUTH_SUPPORT 1
    #define DAUTH_SUPPORT 1
    #define HAVE_POSTPROCESSOR 1
    #define HAVE_MESSAGES 1
    #define UPGRADE_SUPPORT 1
    #define HAVE_INET6 1
    #define _MHD_ITC_SOCKETPAIR 1
    #define MHD_USE_W32_THREADS 1
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0501
    #endif /* _WIN32_WINNT */
    #if _WIN32_WINNT >= 0x0600
        #define HAVE_POLL 1
    #endif /* _WIN32_WINNT >= 0x0600 */
    #define HAVE_WINSOCK2_H 1
    #define HAVE_WS2TCPIP_H 1
    #define HAVE___LSEEKI64 1
    #define HAVE_W32_GMTIME_S 1
    #define HAVE_ASSERT 1
    #define HAVE_CALLOC 1
    #define HAVE_SNPRINTF 1
    #define HAVE_INTTYPES_H 1
    #define HAVE_ERRNO_H 1
    #define HAVE_FCNTL_H 1
    #define HAVE_INTTYPES_H 1
    #define HAVE_LIMITS_H 1
    #define HAVE_MEMORY_H 1
    #define HAVE_SDKDDKVER_H 1
    #define HAVE_STDINT_H 1
    #define HAVE_STDIO_H 1
    #define HAVE_STDLIB_H 1
    #define HAVE_STRINGS_H 1
    #define HAVE_STRING_H 1
    #define HAVE_SYS_STAT_H 1
    #define HAVE_SYS_TYPES_H 1
    #define HAVE_TIME_H 1
    #define HAVE_STDDEF_H 1
    #define HAVE_STDBOOL_H 1
    #define HAVE_WINDOWS_H 1
    #define HAVE___FUNCTION__ 1
    #define HAVE___FUNC__ 1
    #define _GNU_SOURCE  1
    #define STDC_HEADERS 1
    #define __STDC_NO_VLA__ 1
    #define _MHD_static_inline static __forceinline
    #define _MHD_NORETURN __declspec(noreturn)
    #if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__)
        #define SIZEOF_SIZE_T 8
    #else
        #define SIZEOF_SIZE_T 4
    #endif
    #ifdef _USE_32BIT_TIME_T
        #define SIZEOF_STRUCT_TIMEVAL_TV_SEC 4
    #else
        #define SIZEOF_STRUCT_TIMEVAL_TV_SEC 8
    #endif
    #define SIZEOF_UINT64_T 8
    #define SIZEOF_UNSIGNED_INT 4
    #define SIZEOF_UNSIGNED_LONG_LONG 8
#elif defined(__APPLE__)
    #define BAUTH_SUPPORT 1
    #define DAUTH_SUPPORT 1
    #define HAVE_ARPA_INET_H 1
    #define HAVE_ASSERT 1
    #define HAVE_C_VARARRAYS 1
    #define HAVE_CALLOC 1
    #define HAVE_CLOCK_GETTIME 1
    #define HAVE_CLOCK_GET_TIME 1
    #define HAVE_DARWIN_SENDFILE 1
    #define HAVE_DECL_GETSOCKNAME 1
    #define HAVE_DLFCN_H 1
    #define HAVE_ERRNO_H 1
    #define HAVE_FCNTL_H 1
    #define HAVE_FORK 1
    #define HAVE_FSEEKO 1
    #define HAVE_GETSOCKNAME 1
    #define HAVE_GETTIMEOFDAY 1
    #define HAVE_GMTIME_R 1
    #define HAVE_INET6 1
    #define HAVE_INTTYPES_H 1
    #define HAVE_LIBCURL 1
    #define HAVE_LIMITS_H 1
    #define HAVE_MACHINE_ENDIAN_H 1
    #define HAVE_MACHINE_PARAM_H 1
    #define HAVE_MEMMEM 1
    #define HAVE_MEMORY_H 1
    #define HAVE_MESSAGES 1
    #define HAVE_NANOSLEEP 1
    #define HAVE_NETDB_H 1
    #define HAVE_NETINET_IN_H 1
    #define HAVE_NETINET_IP_H 1
    #define HAVE_NETINET_TCP_H 1
    #define HAVE_NET_IF_H 1
    #define HAVE_POLL 1
    #define HAVE_POLL_H 1
    #define HAVE_POSTPROCESSOR 1
    #define HAVE_PREAD 1
    #define HAVE_PTHREAD_H 1
    #define HAVE_PTHREAD_PRIO_INHERIT 1
    #define HAVE_PTHREAD_SETNAME_NP_DARWIN 1
    #define HAVE_PTHREAD_SIGMASK 1
    #define HAVE_RAND 1
    #define HAVE_RANDOM 1
    #define HAVE_REAL_BOOL 1
    #define HAVE_SEARCH_H 1
    #define HAVE_SENDMSG 1
    #define HAVE_SIGNAL_H 1
    #define HAVE_SNPRINTF 1
    #define HAVE_SOCKADDR_IN_SIN_LEN 1
    #define HAVE_STDBOOL_H 1
    #define HAVE_STDDEF_H 1
    #define HAVE_STDINT_H 1
    #define HAVE_STDIO_H 1
    #define HAVE_STDLIB_H 1
    #define HAVE_STRINGS_H 1
    #define HAVE_STRING_H 1
    #define HAVE_SYS_IOCTL_H 1
    #define HAVE_SYS_MMAN_H 1
    #define HAVE_SYS_MSG_H 1
    #define HAVE_SYS_PARAM_H 1
    #define HAVE_SYS_SELECT_H 1
    #define HAVE_SYS_SOCKET_H 1
    #define HAVE_SYS_STAT_H 1
    #define HAVE_SYS_SYSCTL_H 1
    #define HAVE_SYS_TIME_H 1
    #define HAVE_SYS_TYPES_H 1
    #define HAVE_SYSCTL 1
    #define HAVE_SYSCONF 1
    #define HAVE_TIME_H 1
    #define HAVE_TSEARCH 1
    #define HAVE_UNISTD_H 1
    #define HAVE_USLEEP 1
    #define HAVE_WAITPID 1
    #define HAVE_WRITEV 1
    #define HAVE___FUNCTION__ 1
    #define HAVE___FUNC__ 1
    #define INLINE_FUNC 1
    #define OSX 1
    #define LT_OBJDIR ".libs/"
    #define MHD_HAVE___BUILTIN_BSWAP32 1
    #define MHD_HAVE___BUILTIN_BSWAP64 1
    #define MHD_USE_GETSOCKNAME 1
    #define MHD_USE_POSIX_THREADS 1
    #define STDC_HEADERS 1
    #define UPGRADE_SUPPORT 1
    #define _DARWIN_C_SOURCE 1
    #ifndef _DARWIN_USE_64_BIT_INODE
    # define _DARWIN_USE_64_BIT_INODE 1
    #endif
    #define _GNU_SOURCE 1
    #define _MHD_EXTERN __attribute__((visibility("default"))) extern
    #define _MHD_ITC_PIPE 1
    #define _MHD_static_inline static inline __attribute__((always_inline))
    #define _MHD_NORETURN __attribute__((noreturn))
    #define FUNC_ATTR_NOSANITIZE_WORKS 1
    #define FUNC_ATTR_PTRCOMPARE_WOKRS 1
    #if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__)
        #define SIZEOF_SIZE_T 8
        #define SIZEOF_STRUCT_TIMEVAL_TV_SEC 8
    #else
        #define SIZEOF_SIZE_T 4
        #if _TIME_BITS == 64
            #define SIZEOF_STRUCT_TIMEVAL_TV_SEC 8
        #else
            #define SIZEOF_STRUCT_TIMEVAL_TV_SEC 4
        #endif
    #endif
    #define SIZEOF_UINT64_T 8
    #define SIZEOF_UNSIGNED_INT 4
    #define SIZEOF_UNSIGNED_LONG_LONG 8
#elif defined(__linux__)
    #define BAUTH_SUPPORT 1
    #define DAUTH_SUPPORT 1
    #define EPOLL_SUPPORT 1
    #define HAVE_ACCEPT4 1
    #define HAVE_ARPA_INET_H 1
    #define HAVE_ASSERT 1
    #define HAVE_C_VARARRAYS 1
    #define HAVE_CALLOC 1
    #define HAVE_CLOCK_GETTIME 1
    #define HAVE_DLFCN_H 1
    #define HAVE_ENDIAN_H 1
    #define HAVE_EPOLL_CREATE1 1
    #define HAVE_ERRNO_H 1
    #define HAVE_FCNTL_H 1
    #define HAVE_FORK 1
    #define HAVE_FSEEKO 1
    #define HAVE_GETTIMEOFDAY 1
    #define HAVE_GMTIME_R 1
    #define HAVE_INET6 1
    #define HAVE_INTTYPES_H 1
    #define HAVE_LIMITS_H 1
    #define HAVE_LISTEN_SHUTDOWN 1
    #define HAVE_LSEEK64 1
    #define HAVE_MEMMEM 1
    #define HAVE_MEMORY_H 1
    #define HAVE_MESSAGES 1
    #define HAVE_NANOSLEEP 1
    #define HAVE_NETDB_H 1
    #define HAVE_NETINET_IN_H 1
    #define HAVE_NETINET_IP_H 1
    #define HAVE_NETINET_TCP_H 1
    #define HAVE_NET_IF_H 1
    #define HAVE_POLL 1
    #define HAVE_POLL_H 1
    #define HAVE_POSTPROCESSOR 1
    #define HAVE_PREAD 1
    #define HAVE_PREAD64 1
    #define HAVE_PTHREAD_H 1
    #define HAVE_PTHREAD_PRIO_INHERIT 1
    #define HAVE_PTHREAD_SETNAME_NP_GNU 1
    #define HAVE_PTHREAD_SIGMASK 1
    #define HAVE_RAND 1
    #define HAVE_RANDOM 1
    #define HAVE_REAL_BOOL 1
    #define HAVE_SEARCH_H 1
    #define HAVE_SENDMSG 1
    #define HAVE_SENDFILE64 1
    #define HAVE_SIGNAL_H 1
    #define HAVE_SNPRINTF 1
    #define HAVE_SOCK_NONBLOCK 1
    #define HAVE_STDBOOL_H 1
    #define HAVE_STDDEF_H 1
    #define HAVE_STDINT_H 1
    #define HAVE_STDIO_H 1
    #define HAVE_STDLIB_H 1
    #define HAVE_STRINGS_H 1
    #define HAVE_STRING_H 1
    #define HAVE_SYS_EVENTFD_H 1
    #define HAVE_SYS_IOCTL_H 1
    #define HAVE_SYS_MMAN_H 1
    #define HAVE_SYS_MSG_H 1
    #define HAVE_SYS_PARAM_H 1
    #define HAVE_SYS_SELECT_H 1
    #define HAVE_SYS_SOCKET_H 1
    #define HAVE_SYS_STAT_H 1
    #define HAVE_SYS_SYSCTL_H 1
    #define HAVE_SYS_TIME_H 1
    #define HAVE_SYS_TYPES_H 1
    #define HAVE_SYSCTL 1
    #define HAVE_SYSCONF 1
    #define HAVE_TIME_H 1
    #define HAVE_TSEARCH 1
    #define HAVE_UNISTD_H 1
    #define HAVE_USLEEP 1
    #define HAVE_WAITPID 1
    #define HAVE_WRITEV 1
    #define HAVE___FUNCTION__ 1
    #define HAVE___FUNC__ 1
    #define INLINE_FUNC 1
    #define LINUX 1
    #define LT_OBJDIR ".libs/"
    #define MHD_HAVE___BUILTIN_BSWAP32 1
    #define MHD_HAVE___BUILTIN_BSWAP64 1
    #define MHD_USE_POSIX_THREADS 1
    #define STDC_HEADERS 1
    #define UPGRADE_SUPPORT 1
    #define _GNU_SOURCE 1
    #define _MHD_EXTERN __attribute__((visibility("default"))) extern
    #define _MHD_ITC_EVENTFD 1
    #define _MHD_static_inline static inline __attribute__((always_inline))
    #define _MHD_NORETURN __attribute__((noreturn))
    #define _XOPEN_SOURCE 700
    #define FUNC_ATTR_NOSANITIZE_WORKS 1
    #define FUNC_ATTR_PTRCOMPARE_WOKRS 1
    #if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__)
        #define SIZEOF_SIZE_T 8
        #define SIZEOF_STRUCT_TIMEVAL_TV_SEC 8
    #else
        #define SIZEOF_SIZE_T 4
        #define SIZEOF_STRUCT_TIMEVAL_TV_SEC 4
    #endif
    #define SIZEOF_UINT64_T 8
    #define SIZEOF_UNSIGNED_INT 4
    #define SIZEOF_UNSIGNED_LONG_LONG 8
#elif defined(__OpenBSD__)
    #define BAUTH_SUPPORT 1
    #define DAUTH_SUPPORT 1
    #define HAVE_ACCEPT4 1
    #define HAVE_ARPA_INET_H 1
    #define HAVE_CALLOC 1
    #define HAVE_CLOCK_GETTIME 1
    #define HAVE_C_ALIGNOF 1
    #define HAVE_C_VARARRAYS 1
    #define HAVE_DLFCN_H 1
    #define HAVE_ENDIAN_H 1
    #define HAVE_ERRNO_H 1
    #define HAVE_FCNTL_H 1
    #define HAVE_FORK 1
    #define HAVE_FSEEKO 1
    #define HAVE_GETSOCKNAME 1
    #define HAVE_GETTIMEOFDAY 1
    #define HAVE_GMTIME_R 1
    #define HAVE_INET6 1
    #define HAVE_INTTYPES_H 1
    #define HAVE_LIBCURL 1
    #define HAVE_LIMITS_H 1
    #define HAVE_LISTEN_SHUTDOWN 1
    #define HAVE_MACHINE_ENDIAN_H 1
    #define HAVE_MACHINE_PARAM_H 1
    #define HAVE_MEMMEM 1
    #define HAVE_MESSAGES 1
    #define HAVE_NANOSLEEP 1
    #define HAVE_NETDB_H 1
    #define HAVE_NETINET_ICMP_VAR_H 1
    #define HAVE_NETINET_IN_H 1
    #define HAVE_NETINET_IP_H 1
    #define HAVE_NETINET_IP_ICMP_H 1
    #define HAVE_NETINET_TCP_H 1
    #define HAVE_NET_IF_H 1
    #define HAVE_PIPE2_FUNC 1
    #define HAVE_POLL 1
    #define HAVE_POLL_H 1
    #define HAVE_POSTPROCESSOR 1
    #define HAVE_PREAD 1
    #define HAVE_PTHREAD_H 1
    #define HAVE_PTHREAD_NP_H 1
    #define HAVE_PTHREAD_PRIO_INHERIT 1
    #define HAVE_PTHREAD_SET_NAME_NP_FREEBSD 1
    #define HAVE_PTHREAD_SIGMASK 1
    #define HAVE_RANDOM 1
    #define HAVE_REAL_BOOL 1
    #define HAVE_SEARCH_H 1
    #define HAVE_SENDMSG 1
    #define HAVE_SIGNAL_H 1
    #define HAVE_SNPRINTF 1
    #define HAVE_SOCK_NONBLOCK 1
    #define HAVE_STDALIGN_H 1
    #define HAVE_STDBOOL_H 1
    #define HAVE_STDDEF_H 1
    #define HAVE_STDINT_H 1
    #define HAVE_STDIO_H 1
    #define HAVE_STDLIB_H 1
    #define HAVE_STRINGS_H 1
    #define HAVE_STRING_H 1
    #define HAVE_STRUCT_SOCKADDR_IN6_SIN6_LEN 1
    #define HAVE_STRUCT_SOCKADDR_IN_SIN_LEN 1
    #define HAVE_STRUCT_SOCKADDR_STORAGE_SS_LEN 1
    #define HAVE_SYSCONF 1
    #define HAVE_SYSCTL 1
    #define HAVE_SYS_ENDIAN_H 1
    #define HAVE_SYS_IOCTL_H 1
    #define HAVE_SYS_MMAN_H 1
    #define HAVE_SYS_MSG_H 1
    #define HAVE_SYS_PARAM_H 1
    #define HAVE_SYS_SELECT_H 1
    #define HAVE_SYS_SOCKET_H 1
    #define HAVE_SYS_STAT_H 1
    #define HAVE_SYS_SYSCTL_H 1
    #define HAVE_SYS_TIME_H 1
    #define HAVE_SYS_TYPES_H 1
    #define HAVE_TIMESPEC_GET 1
    #define HAVE_TIME_H 1
    #define HAVE_TSEARCH 1
    #define HAVE_TWALK 1
    #define HAVE_UNISTD_H 1
    #define HAVE_USLEEP 1
    #define HAVE_WAITPID 1
    #define HAVE_WRITEV 1
    #define HAVE_ZLIB_H 1
    #define HAVE___FUNC__ 1
    #define INLINE_FUNC 1
    #define OPENBSD 1
    #define SOMEBSD 1
    #define LT_OBJDIR ".libs/"
    #define MHD_HAVE___BUILTIN_BSWAP32 1
    #define MHD_HAVE___BUILTIN_BSWAP64 1
    #define MHD_USE_GETSOCKNAME 1
    #define MHD_USE_PAGE_SIZE_MACRO 1
    #define MHD_USE_PAGE_SIZE_MACRO_STATIC 1
    #define MHD_USE_POSIX_THREADS 1
    #define STDC_HEADERS 1
    #define UPGRADE_SUPPORT 1
    #define _BSD_SOURCE 1
    #define _GNU_SOURCE 1
    #define _MHD_EXTERN __attribute__((visibility("default"))) extern
    #define _MHD_ITC_PIPE 1
    #define _MHD_NORETURN _Noreturn
    #define _MHD_static_inline static inline __attribute__((always_inline))
    #define _XOPEN_SOURCE 700
    #if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__)
        #define SIZEOF_SIZE_T 8
    #else
        #define SIZEOF_SIZE_T 4
    #endif
    #define SIZEOF_STRUCT_TIMEVAL_TV_SEC 4
    #define SIZEOF_UINT64_T 8
    #define SIZEOF_UNSIGNED_INT 4
    #define SIZEOF_UNSIGNED_LONG_LONG 8
#else
    #error "Custom MHD_config.h not suited for this platform"
#endif

#if defined(__clang__)
    #if __has_feature(address_sanitizer)
        #define MHD_ASAN_ACTIVE 1
    #endif
#elif defined(__SANITIZE_ADDRESS__)
    #define MHD_ASAN_ACTIVE 1
#endif
