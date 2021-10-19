/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef HS_LIBHS_H
#define HS_LIBHS_H

/* This file provides both the interface and the implementation.

   To instantiate the implementation,
        #define HS_IMPLEMENTATION
   in *ONE* source file, before #including this file.

   libhs depends on **a few OS-provided libraries** that you need to link:

   OS                  | Dependencies
   ------------------- | --------------------------------------------------------------------------------
   Windows (MSVC)      | Nothing to do, libhs uses `#pragma comment(lib)`
   Windows (MinGW-w64) | Link _user32, advapi32, setupapi and hid_ `-luser32 -ladvapi32 -lsetupapi -lhid`
   OSX (Clang)         | Link _CoreFoundation and IOKit_ `-framework CoreFoundation -framework IOKit`
   Linux (GCC)         | Link _libudev_ `-ludev`

   Other systems are not supported at the moment. */

// common.h
// ------------------------------------

#ifndef HS_COMMON_H
#define HS_COMMON_H

#include <stddef.h>
#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
    #define _HS_BEGIN_C extern "C" {
    #define _HS_END_C }
#else
    #define _HS_BEGIN_C
    #define _HS_END_C
#endif

_HS_BEGIN_C

typedef struct hs_device hs_device;
typedef struct hs_monitor hs_monitor;
typedef struct hs_port hs_port;
typedef struct hs_match_spec hs_match_spec;

/**
 * @defgroup misc Miscellaneous
 * @brief Error management and platform-specific functions.
 */

/**
 * @ingroup misc
 * @brief Compile-time libhs version.
 *
 * The version is represented as a six-digit decimal value respecting **semantic versioning**:
 * MMmmpp (major, minor, patch), e.g. 900 for "0.9.0", 10002 for "1.0.2" or 220023 for "22.0.23".
 *
 * @sa hs_version() for the run-time version.
 * @sa HS_VERSION_STRING for the version string.
 */
#define HS_VERSION 900
/**
 * @ingroup misc
 * @brief Compile-time libhs version string.
 *
 * @sa hs_version_string() for the run-time version.
 * @sa HS_VERSION for the version number.
 */
#define HS_VERSION_STRING "0.9.0"

#if defined(__GNUC__)
    #define _HS_POSSIBLY_UNUSED __attribute__((__unused__))
    #define _HS_THREAD_LOCAL __thread
    #define _HS_ALIGN_OF(type)  __alignof__(type)

    #ifdef __MINGW_PRINTF_FORMAT
        #define _HS_PRINTF_FORMAT(fmt, first) __attribute__((__format__(__MINGW_PRINTF_FORMAT, fmt, first)))
    #else
        #define _HS_PRINTF_FORMAT(fmt, first) __attribute__((__format__(__printf__, fmt, first)))
    #endif
#elif _MSC_VER >= 1900
    #define _HS_POSSIBLY_UNUSED
    #define _HS_THREAD_LOCAL __declspec(thread)
    #define _HS_ALIGN_OF(type) __alignof(type)

    #define _HS_PRINTF_FORMAT(fmt, first)

    // HAVE_SSIZE_T is used this way by other projects
    #ifndef HAVE_SSIZE_T
        #define HAVE_SSIZE_T
        #ifdef _WIN64
typedef __int64 ssize_t;
        #else
typedef long ssize_t;
        #endif
    #endif

    #define strcasecmp _stricmp
    #define strncasecmp _strnicmp
#else
    #error "This compiler is not supported"
#endif

#define _HS_CONCAT_HELPER(a, b) a ## b
#define _HS_CONCAT(a, b) _HS_CONCAT_HELPER(a, b)
#define _HS_UNIQUE_ID(prefix) _HS_CONCAT(prefix, __LINE__)
#define _HS_UNUSED(arg) ((void)(arg))
#define _HS_COUNTOF(a) (sizeof(a) / sizeof(*(a)))
#define _HS_MIN(a,b) ((a) < (b) ? (a) : (b))
#define _HS_MAX(a,b) ((a) > (b) ? (a) : (b))
#define _HS_ALIGN_SIZE(size, align) (((size) + (align) - 1) / (align) * (align))
#define _HS_ALIGN_SIZE_FOR_TYPE(size, type) _HS_ALIGN_SIZE((size), sizeof(type))
#define _HS_CONTAINER_OF(head, type, member) \
    ((type *)((char *)(head) - (size_t)(&((type *)0)->member)))

int _hs_asprintf(char **strp, const char *fmt, ...) _HS_PRINTF_FORMAT(2, 3);
int _hs_vasprintf(char **strp, const char *fmt, va_list ap);

#if defined(DOXYGEN)
/**
 * @ingroup misc
 * @brief Type representing an OS descriptor/handle.
 *
 * This is used in functions taking or returning an OS descriptor/handle, in order to avoid
 * having different function prototypes on different platforms.
 *
 * The underlying type is:
 * - int on POSIX platforms, including OS X
 * - HANDLE (aka. void *) on Windows
 */
typedef _platform_specific_ hs_handle;
#elif defined(_WIN32)
typedef void *hs_handle; // HANDLE
#else
typedef int hs_handle;
#endif

/**
 * @ingroup misc
 * @brief libhs message log levels.
 */
typedef enum hs_log_level {
    /** Fatal errors. */
    HS_LOG_ERROR,
    /** Non-fatal problem. */
    HS_LOG_WARNING,
    /** Internal debug information. */
    HS_LOG_DEBUG
} hs_log_level;

/**
 * @ingroup misc
 * @brief libhs error codes.
 */
typedef enum hs_error_code {
    /** Memory error. */
    HS_ERROR_MEMORY        = -1,
    /** Missing resource error. */
    HS_ERROR_NOT_FOUND     = -2,
    /** Permission denied. */
    HS_ERROR_ACCESS        = -3,
    /** Input/output error. */
    HS_ERROR_IO            = -4,
    /** Parse error. */
    HS_ERROR_PARSE         = -5,
    /** Generic system error. */
    HS_ERROR_SYSTEM        = -6
} hs_error_code;

typedef void hs_log_handler_func(hs_log_level level, int err, const char *msg, void *udata);

/**
 * @{
 * @name Version Functions
 */

/**
 * @ingroup misc
 * @brief Run-time libhs version.
 *
 * The version is represented as a six-digit decimal value respecting **semantic versioning**:
 * MMmmpp (major, minor, patch), e.g. 900 for "0.9.0", 10002 for "1.0.2" or 220023 for "22.0.23".
 *
 * @return This function returns the run-time version number.
 *
 * @sa HS_VERSION for the compile-time version.
 * @sa hs_version_string() for the version string.
 */
uint32_t hs_version(void);
/**
 * @ingroup misc
 * @brief Run-time libhs version string.
 *
 * @return This function returns the run-time version string.
 *
 * @sa HS_VERSION_STRING for the compile-time version.
 * @sa hs_version() for the version number.
 */
const char *hs_version_string(void);

/** @} */

/**
 * @{
 * @name Log Functions
 */

/**
 * @ingroup misc
 * @brief Default log handler, see hs_log_set_handler() for more information.
 */
void hs_log_default_handler(hs_log_level level, int err, const char *msg, void *udata);
/**
 * @ingroup misc
 * @brief Change the log handler function.
 *
 * The default handler prints the message to stderr. It does not print debug messages unless
 * the environment variable LIBHS_DEBUG is set.
 *
 * @param f     New log handler, or hs_log_default_handler to restore the default one.
 * @param udata Pointer to user-defined data for the handler (use NULL for hs_log_default_handler).
 *
 * @sa hs_log()
 * @sa hs_log_default_handler() is the default log handler.
 */
void hs_log_set_handler(hs_log_handler_func *f, void *udata);
/**
 * @ingroup misc
 * @brief Call the log callback with a printf-formatted message.
 *
 * Format a message and call the log callback with it. The default callback prints it to stderr,
 * see hs_log_set_handler(). This callback does not print debug messages unless the environment
 * variable LIBHS_DEBUG is set.
 *
 * @param level Log level.
 * @param fmt Format string, using printf syntax.
 * @param ...
 *
 * @sa hs_log_set_handler() to use a custom callback function.
 */
void hs_log(hs_log_level level, const char *fmt, ...) _HS_PRINTF_FORMAT(2, 3);

/** @} */

/**
 * @{
 * @name Error Functions
 */

/**
 * @ingroup misc
 * @brief Mask an error code.
 *
 * Mask error codes to prevent libhs from calling the log callback (the default one simply prints
 * the string to stderr). It does not change the behavior of the function where the error occurs.
 *
 * For example, if you want to open a device without a missing device message, you can use:
 * @code{.c}
 * hs_error_mask(HS_ERROR_NOT_FOUND);
 * r = hs_port_open(dev, HS_PORT_MODE_RW, &port);
 * hs_error_unmask();
 * if (r < 0)
 *     return r;
 * @endcode
 *
 * The masked codes are kept in a limited stack, you must not forget to unmask codes quickly
 * with @ref hs_error_unmask().
 *
 * @param err Error code to mask.
 *
 * @sa hs_error_unmask()
 * @sa hs_error_is_masked()
 */
void hs_error_mask(hs_error_code err);
/**
 * @ingroup misc
 * @brief Unmask the last masked error code.
 *
 * @sa hs_error_mask()
 */
void hs_error_unmask(void);
/**
 * @ingroup misc
 * @brief Check whether an error code is masked.
 *
 * Returns 1 if error code @p err is currently masked, or 0 otherwise.
 *
 * hs_error() does not call the log handler for masked errors, you only need to use
 * this function if you want to bypass hs_error() and call hs_log() directly.
 *
 * @param err Error code to check.
 *
 * @sa hs_error_mask()
 */
int hs_error_is_masked(int err);

/**
  * @ingroup misc
  * @brief Get the last error message emitted on the current thread.
  */
const char *hs_error_last_message();

/**
 * @ingroup misc
 * @brief Call the log callback with a printf-formatted error message.
 *
 * Format an error message and call the log callback with it. Pass NULL to @p fmt to use a
 * generic error message. The default callback prints it to stderr, see hs_log_set_handler().
 *
 * The error code is simply returned as a convenience, so you can use this function like:
 * @code{.c}
 * // Explicit error message
 * int pipe[2], r;
 * r = pipe(pipe);
 * if (r < 0)
 *     return hs_error(HS_ERROR_SYSTEM, "pipe() failed: %s", strerror(errno));
 *
 * // Generic error message (e.g. "Memory error")
 * char *p = malloc(128);
 * if (!p)
 *     return hs_error(HS_ERROR_MEMORY, NULL);
 * @endcode
 *
 * @param err Error code.
 * @param fmt Format string, using printf syntax.
 * @param ...
 * @return This function returns the error code.
 *
 * @sa hs_error_mask() to mask specific error codes.
 * @sa hs_log_set_handler() to use a custom callback function.
 */
int hs_error(hs_error_code err, const char *fmt, ...) _HS_PRINTF_FORMAT(2, 3);

_HS_END_C

#endif

// array.h
// ------------------------------------

#ifndef HS_ARRAY_H
#define HS_ARRAY_H

// #include "common.h"

_HS_BEGIN_C

struct _hs_array {
    void *values;
    size_t allocated;
    size_t count;
};

#define _HS_ARRAY(Type) \
    struct { \
        Type *values; \
        size_t allocated; \
        size_t count; \
    }

#define _hs_array_release(Array) \
    _hs_array_release_((struct _hs_array *)&(Array)->values)

#define _hs_array_grow(Array, Need) \
    _hs_array_grow_((struct _hs_array *)&(Array)->values, sizeof(*(Array)->values), (Need))
#define _hs_array_push(Array, Value) \
    (_hs_array_grow((Array), 1) < 0 \
        ? HS_ERROR_MEMORY \
        : (((Array)->values[(Array)->count++] = (Value)), 0))

#define _hs_array_shrink(Array) \
    _hs_array_shrink_((struct _hs_array *)&(Array)->values, sizeof(*(Array)->values))
#define _hs_array_pop(Array, Count) \
    do { \
        (Array)->count -= (Count); \
        if ((Array)->count <= (Array)->allocated / 2) \
            _hs_array_shrink(Array); \
    } while (0)
#define _hs_array_remove(Array, Offset, Count) \
    do { \
        size_t _HS_UNIQUE_ID(start) = (Offset); \
        size_t _HS_UNIQUE_ID(count) = (Count); \
        size_t _HS_UNIQUE_ID(end) = _HS_UNIQUE_ID(start) + _HS_UNIQUE_ID(count); \
        memmove((Array)->values + _HS_UNIQUE_ID(start), \
                (Array)->values + _HS_UNIQUE_ID(end), \
                ((Array)->count - _HS_UNIQUE_ID(end)) * sizeof(*(Array)->values)); \
        _hs_array_pop((Array), _HS_UNIQUE_ID(count)); \
    } while (0)

#define _hs_array_move(Src, Dest) \
    do { \
        (Dest)->values = (Src)->values; \
        (Dest)->count = (Src)->count; \
        (Dest)->allocated = (Src)->allocated; \
        memset((Src), 0, sizeof(*(Src))); \
    } while (0)

void _hs_array_release_(struct _hs_array *array);

int _hs_array_expand_(struct _hs_array *array, size_t value_size, size_t need);
static inline int _hs_array_grow_(struct _hs_array *array, size_t value_size, size_t need)
{
    if (need > array->allocated - array->count) {
        return _hs_array_expand_(array, value_size, need);
    } else {
        return 0;
    }
}

void _hs_array_shrink_(struct _hs_array *array, size_t value_size);

_HS_END_C

#endif

// htable.h
// ------------------------------------

#ifndef HS_HTABLE_H
#define HS_HTABLE_H

// #include "common.h"

_HS_BEGIN_C

typedef struct _hs_htable_head {
    // Keep first!
    struct _hs_htable_head *next;
    uint32_t key;
} _hs_htable_head;

typedef struct _hs_htable {
    unsigned int size;
    void **heads;
} _hs_htable;

int _hs_htable_init(_hs_htable *table, unsigned int size);
void _hs_htable_release(_hs_htable *table);

_hs_htable_head *_hs_htable_get_head(_hs_htable *table, uint32_t key);

void _hs_htable_add(_hs_htable *table, uint32_t key, _hs_htable_head *head);
void _hs_htable_insert(_hs_htable_head *prev, _hs_htable_head *head);
void _hs_htable_remove(_hs_htable_head *head);

void _hs_htable_clear(_hs_htable *table);

static inline uint32_t _hs_htable_hash_str(const char *s)
{
    uint32_t hash = 0;
    while (*s)
        hash = hash * 101 + (unsigned char)*s++;

    return hash;
}

static inline uint32_t _hs_htable_hash_ptr(const void *p)
{
    return (uint32_t)((uintptr_t)p >> 3);
}

/* While a break will only end the inner loop, the outer loop will subsequently fail
   the cur == HS_UNIQUE_ID(head) test and thus break out of the outer loop too. */
#define _hs_htable_foreach(cur, table) \
    for (_hs_htable_head *_HS_UNIQUE_ID(head) = (_hs_htable_head *)(table)->heads, *cur = _HS_UNIQUE_ID(head), *_HS_UNIQUE_ID(next); \
            cur == _HS_UNIQUE_ID(head) && _HS_UNIQUE_ID(head) < (_hs_htable_head *)((table)->heads + (table)->size); \
            _HS_UNIQUE_ID(head) = (_hs_htable_head *)((_hs_htable_head **)_HS_UNIQUE_ID(head) + 1), cur = (_hs_htable_head *)((_hs_htable_head **)cur + 1)) \
        for (cur = cur->next, _HS_UNIQUE_ID(next) = cur->next; cur != _HS_UNIQUE_ID(head); cur = _HS_UNIQUE_ID(next), _HS_UNIQUE_ID(next) = cur->next) \

#define _hs_htable_foreach_hash(cur, table, k) \
    if ((table)->size) \
        for (_hs_htable_head *_HS_UNIQUE_ID(head) = _hs_htable_get_head((table), (k)), *cur = _HS_UNIQUE_ID(head)->next, *_HS_UNIQUE_ID(next) = cur->next; \
             cur != _HS_UNIQUE_ID(head); cur = _HS_UNIQUE_ID(next), _HS_UNIQUE_ID(next) = cur->next) \
            if (cur->key == (k))

_HS_END_C

#endif

// device.h
// ------------------------------------

#ifndef HS_DEVICE_H
#define HS_DEVICE_H

// #include "common.h"
// #include "htable.h"

_HS_BEGIN_C

/**
 * @defgroup device Device handling
 * @brief Access device information and open device handles.
 */

/**
 * @ingroup device
 * @brief Current device status.
 *
 * The device status can only change when hs_monitor_refresh() is called.
 *
 * @sa hs_device
 */
typedef enum hs_device_status {
    /** Device is connected and ready. */
    HS_DEVICE_STATUS_ONLINE = 1,
    /** Device has been disconnected. */
    HS_DEVICE_STATUS_DISCONNECTED
} hs_device_status;

/**
 * @ingroup device
 * @brief Device type.
 *
 * @sa hs_device
 * @sa hs_device_type_strings
 */
typedef enum hs_device_type {
    /** HID device. */
    HS_DEVICE_TYPE_HID = 1,
    /** Serial device. */
    HS_DEVICE_TYPE_SERIAL
} hs_device_type;

/**
 * @ingroup device
 * @brief Device type strings
 *
 * Use hs_device_type_strings[dev->type] to get a string representation:
 * - HS_DEVICE_TYPE_HID = "hid"
 * - HS_DEVICE_TYPE_SERIAL = "serial"
 *
 * @sa hs_device_type
 */
static const char *const hs_device_type_strings[] = {
    NULL,
    "hid",
    "serial"
};

/**
 * @ingroup device
 * @brief The hs_device struct
 *
 * Members omitted from the list below are reserved for internal use.
 */
struct hs_device {
    /** @cond */
    unsigned int refcount;
    _hs_htable_head hnode;
    char *key;
    /** @endcond */

    /** Device type, see @ref hs_device_type. */
    hs_device_type type;
    /** Current device status, see @ref hs_device_status. */
    hs_device_status status;
    /**
     * @brief Device location.
     *
     * The location is bus-specific:
     * - **USB** = "usb-<root_hub_id>[-<port_id>]+" (e.g. "usb-2-5-4")
     */
    char *location;
    /**
     * @brief Get the device node path.
     *
     * This may not always be a real filesystem path, for example on OS X HID devices cannot be
     * used through a device node.
     */
    char *path;
    /** Device vendor identifier. */
    uint16_t vid;
    /** Device product identifier. */
    uint16_t pid;
    /** Device bcd. */
    uint16_t bcd_device;
    /** Device manufacturer string, or NULL if not available. */
    char *manufacturer_string;
    /** Device product string, or NULL if not available. */
    char *product_string;
    /** Device serial number string, or NULL if not available. */
    char *serial_number_string;
    /** Device interface number. */
    uint8_t iface_number;

    /** Match pointer, copied from udata in @ref hs_match_spec. */
    void *match_udata;

    /** Contains type-specific information, see below. */
    union {
        /** Only valid when type == HS_DEVICE_TYPE_HID. */
        struct {
            /** Primary usage page value read from the HID report descriptor. */
            uint16_t usage_page;
            /** Primary usage value read from the HID report descriptor. */
            uint16_t usage;

#if defined(_WIN32)
            /** @cond */
            size_t input_report_len;
            /** @endcond */
#elif defined(__linux__)
            /** @cond */
            // Needed to work around a bug on old Linux kernels
            bool numbered_reports;
            /** @endcond */
#endif
        } hid;
    } u;
};

/**
 * @ingroup device
 * @brief Device open mode.
 *
 * @sa hs_port_open()
 */
typedef enum hs_port_mode {
    /** Open device for reading. */
    HS_PORT_MODE_READ  = 1,
    /** Open device for writing. */
    HS_PORT_MODE_WRITE = 2,
    /** Open device for read/write operations. */
    HS_PORT_MODE_RW    = 3
} hs_port_mode;

/**
 * @ingroup device
 * @typedef hs_port
 * @brief Opaque structure representing a device I/O port.
 *
 */
struct hs_port;

/**
 * @{
 * @name Resource management
 */

/**
 * @ingroup device
 * @brief Increase the device reference count.
 *
 * This function is thread-safe.
 *
 * @param dev Device object.
 * @return This function returns the device object, for convenience.
 */
hs_device *hs_device_ref(hs_device *dev);
/**
 * @ingroup device
 * @brief Decrease the device reference count.
 *
 * When the reference count reaches 0, the device object is freed. This function is thread-safe.
 *
 * @param dev Device object.
 */
void hs_device_unref(hs_device *dev);

/**
  * @{
  * @name Handle Functions
  */

/**
 * @ingroup device
 * @brief Open a device.
 *
 * The handle object keeps a refcounted reference to the device object, you are free to drop
 * your own reference.
 *
 * @param      dev   Device object to open.
 * @param      mode  Open device for read / write or both.
 * @param[out] rport Device handle, the value is changed only if the function succeeds.
 * @return This function returns 0 on success, or a negative @ref hs_error_code value.
 */
int hs_port_open(hs_device *dev, hs_port_mode mode, hs_port **rport);
/**
 * @ingroup device
 * @brief Close a device, and free all used resources.
 *
 * @param port Device handle.
 */
void hs_port_close(hs_port *port);

/**
 * @ingroup device
 * @brief Get the device object from which this handle was opened.
 *
 * @param port Device handle.
 * @return Device object.
 */
hs_device *hs_port_get_device(const hs_port *port);
/**
 * @ingroup device
 * @brief Get a pollable device handle.
 *
 * @ref hs_handle is a typedef to the platform descriptor type: int on POSIX platforms,
 * HANDLE on Windows.
 *
 * You can use this descriptor with select()/poll() on POSIX platforms and the Wait
 * (e.g. WaitForSingleObject()) functions on Windows to know when the device input buffer contains
 * data.
 *
 * Note that this descriptor may not be the real device descriptor on some platforms. For
 * HID devices on OSX, this is actually a pipe that gets signalled when IOHIDDevice gives
 * libhs a report on the background thread.
 *
 * @param port Device handle.
 * @return This function returns a pollable handle.
 *
 * @sa hs_handle
 */
hs_handle hs_port_get_poll_handle(const hs_port *port);

_HS_END_C

#endif

// hid.h
// ------------------------------------

#ifndef HS_HID_H
#define HS_HID_H

// #include "common.h"

_HS_BEGIN_C

/**
 * @defgroup hid HID device I/O
 * @brief Send and receive HID reports (input, output, feature) to and from HID devices.
 */

/**
 * @ingroup hid
 * @brief Read an input report from the device.
 *
 * The first byte will contain the report ID, or 0 if the device does not use numbered reports.
 * HID is message-oriented, if the buffer is too small the extra bytes will be discarded.
 *
 * If no report is available, the function waits for up to @p timeout milliseconds. Use a
 * negative value to wait indefinitely.
 *
 * @param      port    Device handle.
 * @param[out] buf     Input report buffer.
 * @param      size    Size of the report buffer (make room for the report ID).
 * @param      timeout Timeout in milliseconds, or -1 to block indefinitely.
 *
 * @return This function returns the size of the report in bytes + 1 (report ID). It
 *     returns 0 on timeout, or a negative @ref hs_error_code value.
 */
ssize_t hs_hid_read(hs_port *port, uint8_t *buf, size_t size, int timeout);
/**
 * @ingroup hid
 * @brief Send an output report to the device.
 *
 * The first byte must be the report ID, or 0 if the device does not use report IDs.
 *
 * @param port Device handle.
 * @param buf  Output report data.
 * @param size Output report size (including the report ID byte).
 *
 * @return This function returns the size of the report in bytes + 1 (report ID),
 *     or a negative error code.
 */
ssize_t hs_hid_write(hs_port *port, const uint8_t *buf, size_t size);

/**
 * @ingroup hid
 * @brief Get a feature report from the device.
 *
 * The first byte will contain the report ID, or 0 if the device does not use numbered reports.
 *
 * @param      port      Device handle.
 * @param      report_id Specific report to retrieve, or 0 if the device does not use
 *     numbered reports.
 * @param[out] buf       Feature report buffer (make room for the report ID).
 * @param      size      Size of the report buffer.
 *
 * @return This function returns the size of the report in bytes + 1 (report ID),
 *     or a negative @ref hs_error_code value.
 */
ssize_t hs_hid_get_feature_report(hs_port *port, uint8_t report_id, uint8_t *buf, size_t size);
/**
 * @ingroup hid
 * @brief Send a feature report to the device.
 *
 * The first byte must be the report ID, or 0 if the device does not use numbered reports.
 *
 * @param port Device handle.
 * @param buf  Output report data.
 * @param size Output report size (including the report ID byte).
 *
 * @return This function returns the size of the report in bytes + 1 (report ID),
 *     or a negative @ref hs_error_code value.
 */
ssize_t hs_hid_send_feature_report(hs_port *port, const uint8_t *buf, size_t size);

_HS_END_C

#endif

// match.h
// ------------------------------------

#ifndef HS_MATCH_H
#define HS_MATCH_H

// #include "common.h"

_HS_BEGIN_C

/**
 * @defgroup match Device matching
 * @brief Match specific devices on enumeration and hotplug events.
 */

/**
 * @ingroup match
 * @brief Device match specifier, use the @ref HS_MATCH_TYPE "dedicated macros" for convenience.
 *
 * Here is a short example to enumerate all serial devices and HID devices with a specific
 * VID:PID pair.
 *
 * @code{.c}
 * const hs_match_spec specs[] = {
 *     HS_MATCH_TYPE(HS_DEVICE_TYPE_SERIAL, NULL),
 *     HS_MATCH_TYPE_VID_PID(HS_DEVICE_TYPE_HID, 0x16C0, 0x478, NULL)
 * };
 *
 * r = hs_enumerate(specs, sizeof(specs) / sizeof(*specs), device_callback, NULL);
 * @endcode
 */
struct hs_match_spec {
    /** Device type @ref hs_device_type or 0 to match all types. */
    unsigned int type;

    /** Device vendor ID or 0 to match all. */
    uint16_t vid;
    /** Device product ID or 0 to match all. */
    uint16_t pid;

    /** This value will be copied to dev->match_udata, see @ref hs_device. */
    void *udata;
};

/**
 * @ingroup match
 * @brief Match a specific device type, see @ref hs_device_type.
 */
#define HS_MATCH_TYPE(type, udata) {(type), 0, 0, (udata)}
/**
 * @ingroup match
 * @brief Match devices with corresponding VID:PID pair.
 */
#define HS_MATCH_VID_PID(vid, pid, udata) {0, (vid), (pid), (udata)}
/**
 * @ingroup match
 * @brief Match devices with corresponding @ref hs_device_type and VID:PID pair.
 */
#define HS_MATCH_TYPE_VID_PID(type, vid, pid, udata) {(type), (vid), (pid), (udata)}

/**
 * @ingroup match
 * @brief Create device match from human-readable string.
 *
 * Match string  | Details
 * ------------- | -------------------------------------------------
 * 0:0           | Match all devices
 * 0:0/serial    | Match all serial devices
 * abcd:0123/hid | Match HID devices with VID:PID pair 0xABCD:0x0123
 * 0123:abcd     | Match devices with VID:PID pair 0x0123:0xABCD
 *
 * @param      str    Human-readable match string.
 * @param[out] rmatch A pointer to the variable that receives the device match specifier,
 *     it will stay unchanged if the function fails.
 * @return This function returns 0 on success, or a negative @ref hs_error_code value.
 */
int hs_match_parse(const char *str, hs_match_spec *rspec);

_HS_END_C

#endif

// monitor.h
// ------------------------------------

#ifndef HS_MONITOR_H
#define HS_MONITOR_H

// #include "common.h"

_HS_BEGIN_C

/**
 * @defgroup monitor Device discovery
 * @brief Discover devices and react when devices are added and removed.
 */

/**
 * @ingroup monitor
 * @typedef hs_monitor
 * @brief Opaque structure representing a device monitor.
 */
struct hs_monitor;

/**
 * @ingroup monitor
 * @brief Device enumeration and event callback.
 *
 * When refreshing, use hs_device_get_status() to distinguish between added and removed events.
 *
 * You must return 0 to continue the enumeration or event processing. Non-zero values stop the
 * process and are returned from the enumeration/refresh function. You should probably use
 * negative values for errors (@ref hs_error_code) and positive values otherwise, but this is not
 * enforced.
 *
 * @param dev   Device object.
 * @param udata Pointer to user-defined arbitrary data.
 * @return Return 0 to continue the enumeration/event processing, or any other value to abort.
 *     The enumeration/refresh function will then return this value unchanged.
 *
 * @sa hs_enumerate() for simple device enumeration.
 * @sa hs_monitor_refresh() for hotplug support.
 */
typedef int hs_enumerate_func(struct hs_device *dev, void *udata);

/**
 * @{
 * @name Enumeration Functions
 */

/**
 * @ingroup monitor
 * @brief Enumerate current devices.
 *
 * If you need to support hotplugging you should probably use a monitor instead.
 *
 * @param matches Array of device matches, or NULL to enumerate all supported devices.
 * @param count   Number of elements in @p matches.
 * @param f       Callback called for each enumerated device.
 * @param udata   Pointer to user-defined arbitrary data for the callback.
 * @return This function returns 0 on success, or a negative @ref hs_error_code value. If the
 *     callback returns a non-zero value, the enumeration is interrupted and the value is returned.
 *
 * @sa hs_match_spec Match specific devices.
 * @sa hs_enumerate_func() for more information about the callback.
 */
int hs_enumerate(const hs_match_spec *matches, unsigned int count, hs_enumerate_func *f, void *udata);

/**
 * @ingroup monitor
 * @brief Find the first matching device.
 *
 * Don't forget to call hs_device_unref() when you don't need the device object anymore.
 *
 * @param      matches Array of device matches, or NULL to enumerate all supported devices.
 * @param      count   Number of elements in @p matches.
 * @param[out] rdev    Device object, the value is changed only if a device is found.
 * @return This function returns 1 if a device is found, 0 if not or a negative @ref hs_error_code
 *     value.
 */
int hs_find(const hs_match_spec *matches, unsigned int count, struct hs_device **rdev);

/**
 * @{
 * @name Monitoring Functions
 */

/**
 * @ingroup monitor
 * @brief Open a new device monitor.
 *
 * @param      matches  Array of device matches, or NULL to enumerate all supported devices.
 *     This array is not copied and must remain valid until hs_monitor_free().
 * @param      count    Number of elements in @p matches.
 * @param[out] rmonitor A pointer to the variable that receives the device monitor, it will stay
 *     unchanged if the function fails.
 * @return This function returns 0 on success, or a negative @ref hs_error_code value.
 *
 * @sa hs_monitor_free()
 */
int hs_monitor_new(const hs_match_spec *matches, unsigned int count, hs_monitor **rmonitor);
/**
 * @ingroup monitor
 * @brief Close a device monitor.
 *
 * You should not keep any device object or handles beyond this call. In practice, call this at
 * the end of your program.
 *
 * @param monitor Device monitor.
 *
 * @sa hs_monitor_new()
 */
void hs_monitor_free(hs_monitor *monitor);

/**
 * @ingroup monitor
 * @brief Get a pollable descriptor for device monitor events.
 *
 * @ref hs_handle is a typedef to the platform descriptor type: int on POSIX platforms,
 * HANDLE on Windows.
 *
 * You can use this descriptor with select()/poll() on POSIX platforms and the Wait
 * (e.g. WaitForSingleObject()) functions on Windows to know when there are pending device events.
 * Cross-platform facilities are provided to ease this, see @ref hs_poll.
 *
 * Call hs_monitor_refresh() to process events.
 *
 * @param monitor Device monitor.
 * @return This function returns a pollable descriptor, call hs_monitor_refresh() when it
 *     becomes ready.
 *
 * @sa hs_handle
 * @sa hs_monitor_refresh()
 */
hs_handle hs_monitor_get_poll_handle(const hs_monitor *monitor);

/**
 * @ingroup monitor
 * @brief Start listening to OS notifications.
 *
 * This function lists current devices and connects to the OS device manager for device change
 * notifications.
 *
 * You can use hs_monitor_get_poll_handle() to get a pollable descriptor (int on POSIX, HANDLE
 * on Windows). This descriptors becomes ready (POLLIN) when there are notifications, you can then
 * call hs_monitor_refresh() to process them.
 *
 * @param monitor Device monitor.
 * @return This function returns 0 on success, or a negative @ref hs_error_code value.
 */
int hs_monitor_start(hs_monitor *monitor);
/**
 * @ingroup monitor
 * @brief Stop listening to OS notifications.
 *
 * @param monitor Device monitor.
 */
void hs_monitor_stop(hs_monitor *monitor);

/**
 * @ingroup monitor
 * @brief Refresh the device list and fire device change events.
 *
 * Process all the pending device change events to refresh the device list and call the
 * callback for each event.
 *
 * This function is non-blocking.
 *
 * @param monitor Device monitor.
 * @param f       Callback to process each device event, or NULL.
 * @param udata   Pointer to user-defined arbitrary data for the callback.
 * @return This function returns 0 on success, or a negative @ref hs_error_code value. If the
 *     callback returns a non-zero value, the refresh is interrupted and the value is returned.
 *
 * @sa hs_enumerate_func() for more information about the callback.
 */
int hs_monitor_refresh(hs_monitor *monitor, hs_enumerate_func *f, void *udata);

/**
 * @ingroup monitor
 * @brief List the currently known devices.
 *
 * The device list is refreshed when the monitor is started, and when hs_monitor_refresh() is
 * called. This function simply uses the monitor's internal device list.
 *
 * @param monitor Device monitor.
 * @param f       Device enumeration callback.
 * @param udata   Pointer to user-defined arbitrary data for the callback.
 * @return This function returns 0 on success, or a negative @ref hs_error_code value. If the
 *     callback returns a non-zero value, the listing is interrupted and the value is returned.
 *
 * @sa hs_monitor_refresh()
 * @sa hs_enumerate_func() for more information about the callback.
 */
int hs_monitor_list(hs_monitor *monitor, hs_enumerate_func *f, void *udata);

_HS_END_C

#endif

// platform.h
// ------------------------------------

#ifndef HS_PLATFORM_H
#define HS_PLATFORM_H

// #include "common.h"

_HS_BEGIN_C

#ifdef _WIN32
/**
 * @ingroup misc
 * @brief Common Windows version numbers.
 */
enum hs_win32_release {
    /** Windows 2000. */
    HS_WIN32_VERSION_2000 = 500,
    /** Windows XP. */
    HS_WIN32_VERSION_XP = 501,
    /** Windows Server 2003 or XP-64. */
    HS_WIN32_VERSION_2003 = 502,
    /** Windows Vista. */
    HS_WIN32_VERSION_VISTA = 600,
    /** Windows 7 */
    HS_WIN32_VERSION_7 = 601,
    /** Windows 8 */
    HS_WIN32_VERSION_8 = 602,
    /** Windows 8.1 */
    HS_WIN32_VERSION_8_1 = 603,
    /** Windows 10 */
    HS_WIN32_VERSION_10 = 1000
};
#endif

/**
 * @ingroup misc
 * @brief Poll descriptor.
 */
typedef struct hs_poll_source {
    /** OS-specific descriptor. */
    hs_handle desc;
    /** Custom pointer. */
    void *udata;

    /** Boolean output member for ready/signaled state. */
    int ready;
} hs_poll_source;

/**
 * @ingroup misc
 * @brief Maximum number of pollable descriptors.
 */
#define HS_POLL_MAX_SOURCES 64

/**
 * @{
 * @name System Functions
 */

/**
 * @ingroup misc
 * @brief Get time from a monotonic clock.
 *
 * You should not rely on the absolute value, whose meaning may differ on various platforms.
 * Use it to calculate periods and durations.
 *
 * While the returned value is in milliseconds, the resolution is not that good on some
 * platforms. On Windows, it is over 10 milliseconds.
 *
 * @return This function returns a mononotic time value in milliseconds.
 */
uint64_t hs_millis(void);

/**
 * @ingroup misc
 * @brief Suspend execution of calling thread until specified time has passed.
 *
 * @param ms Wait time in milliseconds.
 *
 */
void hs_delay(unsigned int ms);

/**
 * @ingroup misc
 * @brief Adjust a timeout over a time period.
 *
 * This function returns -1 if the timeout is negative. Otherwise, it decreases the timeout
 * for each millisecond elapsed since @p start. When @p timeout milliseconds have passed,
 * the function returns 0.
 *
 * hs_millis() is used as the time source, so you must use it for @p start.
 *
 * @code{.c}
 * uint64_t start = hs_millis();
 * do {
 *     r = poll(&pfd, 1, hs_adjust_timeout(timeout, start));
 * } while (r < 0 && errno == EINTR);
 * @endcode
 *
 * This function is mainly used in libhs to restart interrupted system calls with
 * timeouts, such as poll().
 *
 * @param timeout Timeout is milliseconds.
 * @param start Start of the timeout period, from hs_millis().
 *
 * @return This function returns the adjusted value, or -1 if @p timeout is negative.
 */
int hs_adjust_timeout(int timeout, uint64_t start);

#ifdef __linux__
/**
 * @ingroup misc
 * @brief Get the Linux kernel version as a composite decimal number.
 *
 * For pre-3.0 kernels, the value is MMmmrrppp (2.6.34.1 => 020634001). For recent kernels,
 * it is MMmm00ppp (4.1.2 => 040100002).
 *
 * Do not rely on this too much, because kernel versions do not reflect the different kernel
 * flavors. Some distributions use heavily-patched builds, with a lot of backported code. When
 * possible, detect functionalities instead.
 *
 * @return This function returns the version number.
 */
uint32_t hs_linux_version(void);
#endif

#ifdef _WIN32
/**
 * @ingroup misc
 * @brief Format an error string using FormatMessage().
 *
 * The content is only valid until the next call to hs_win32_strerror(), be careful with
 * multi-threaded code.
 *
 * @param err Windows error code, or use 0 to get it from GetLastError().
 * @return This function returns a private buffer containing the error string, valid until the
 *     next call to hs_win32_strerror().
 */
const char *hs_win32_strerror(unsigned long err);
/**
 * @ingroup misc
 * @brief Get the Windows version as a composite decimal number.
 *
 * The value is MMmm, see https://msdn.microsoft.com/en-us/library/windows/desktop/ms724832%28v=vs.85%29.aspx
 * for the operating system numbers. You can use the predefined enum values from
 * @ref hs_win32_release.
 *
 * Use this only when testing for functionality is not possible or impractical.
 *
 * @return This function returns the version number.
 */
uint32_t hs_win32_version(void);
#endif

#ifdef __APPLE__
/**
 * @ingroup misc
 * @brief Get the Darwin version on Apple systems
 *
 * The value is MMmmrr (11.4.2 => 110402), see https://en.wikipedia.org/wiki/Darwin_%28operating_system%29
 * for the corresponding OS X releases.
 *
 * @return This function returns the version number.
 */
uint32_t hs_darwin_version(void);
#endif

/**
 * @ingroup misc
 * @brief Wait for ready/readable descriptors.
 *
 * This function is provided for convenience, but it is neither fast nor powerful. Use more
 * advanced event libraries (libev, libevent, libuv) or OS-specific functions if you need more.
 *
 * The @p ready field of each source is set to 1 for ready/signaled descriptors, and 0 for
 * all others.
 *
 * This function cannot process more than @ref HS_POLL_MAX_SOURCES sources.
 *
 * @param[in,out] sources Array of descriptor sources.
 * @param         count   Number of sources.
 * @param         timeout Timeout in milliseconds, or -1 to block indefinitely.
 * @return This function returns the number of ready descriptors, 0 on timeout, or a negative
 *     @ref hs_error_code value.
 *
 * @sa hs_poll_source
 */
int hs_poll(hs_poll_source *sources, unsigned int count, int timeout);

_HS_END_C

#endif

// serial.h
// ------------------------------------

#ifndef HS_SERIAL_H
#define HS_SERIAL_H

// #include "common.h"

_HS_BEGIN_C

/**
 * @defgroup serial Serial device I/O
 * @brief Send and receive bytes to and from serial devices.
 */

/**
 * @ingroup serial
 * @brief Supported serial baud rates.
 *
 * @sa hs_serial_config
 */
enum hs_serial_rate {
    /** 110 bps. */
    HS_SERIAL_RATE_110    = 110,
    /** 134 bps. */
    HS_SERIAL_RATE_134    = 134,
    /** 150 bps. */
    HS_SERIAL_RATE_150    = 150,
    /** 200 bps. */
    HS_SERIAL_RATE_200    = 200,
    /** 300 bps. */
    HS_SERIAL_RATE_300    = 300,
    /** 600 bps. */
    HS_SERIAL_RATE_600    = 600,
    /** 1200 bps. */
    HS_SERIAL_RATE_1200   = 1200,
    /** 1800 bps. */
    HS_SERIAL_RATE_1800   = 1800,
    /** 2400 bps. */
    HS_SERIAL_RATE_2400   = 2400,
    /** 4800 bps. */
    HS_SERIAL_RATE_4800   = 4800,
    /** 9600 bps. */
    HS_SERIAL_RATE_9600   = 9600,
    /** 19200 bps. */
    HS_SERIAL_RATE_19200  = 19200,
    /** 38400 bps. */
    HS_SERIAL_RATE_38400  = 38400,
    /** 57600 bps. */
    HS_SERIAL_RATE_57600  = 57600,
    /** 115200 bps. */
    HS_SERIAL_RATE_115200 = 115200,
    /** 230400 bps. */
    HS_SERIAL_RATE_230400 = 230400
};

/**
 * @ingroup serial
 * @brief Supported serial parity modes.
 *
 * @sa hs_serial_config
 */
typedef enum hs_serial_config_parity {
    /** Leave this setting unchanged. */
    HS_SERIAL_CONFIG_PARITY_INVALID = 0,

    /** No parity. */
    HS_SERIAL_CONFIG_PARITY_OFF,
    /** Even parity. */
    HS_SERIAL_CONFIG_PARITY_EVEN,
    /** Odd parity. */
    HS_SERIAL_CONFIG_PARITY_ODD,
    /** Mark parity. */
    HS_SERIAL_CONFIG_PARITY_MARK,
    /** Space parity. */
    HS_SERIAL_CONFIG_PARITY_SPACE
} hs_serial_config_parity;

/**
 * @ingroup serial
 * @brief Supported RTS pin modes and RTS/CTS flow control.
 *
 * @sa hs_serial_config
 */
typedef enum hs_serial_config_rts {
    /** Leave this setting unchanged. */
    HS_SERIAL_CONFIG_RTS_INVALID = 0,

    /** Disable RTS pin. */
    HS_SERIAL_CONFIG_RTS_OFF,
    /** Enable RTS pin. */
    HS_SERIAL_CONFIG_RTS_ON,
    /** Use RTS/CTS pins for flow control. */
    HS_SERIAL_CONFIG_RTS_FLOW
} hs_serial_config_rts;

/**
 * @ingroup serial
 * @brief Supported DTR pin modes.
 *
 * @sa hs_serial_config
 */
typedef enum hs_serial_config_dtr {
    /** Leave this setting unchanged. */
    HS_SERIAL_CONFIG_DTR_INVALID = 0,

    /** Disable DTR pin. */
    HS_SERIAL_CONFIG_DTR_OFF,
    /** Enable DTR pin. This is done by default when a device is opened. */
    HS_SERIAL_CONFIG_DTR_ON
} hs_serial_config_dtr;

/**
 * @ingroup serial
 * @brief Supported serial XON/XOFF (software) flow control modes.
 *
 * @sa hs_serial_config
 */
typedef enum hs_serial_config_xonxoff {
    /** Leave this setting unchanged. */
    HS_SERIAL_CONFIG_XONXOFF_INVALID = 0,

    /** Disable XON/XOFF flow control. */
    HS_SERIAL_CONFIG_XONXOFF_OFF,
    /** Enable XON/XOFF flow control for input only. */
    HS_SERIAL_CONFIG_XONXOFF_IN,
    /** Enable XON/XOFF flow control for output only. */
    HS_SERIAL_CONFIG_XONXOFF_OUT,
    /** Enable XON/XOFF flow control for input and output. */
    HS_SERIAL_CONFIG_XONXOFF_INOUT
} hs_serial_config_xonxoff;

/**
 * @ingroup serial
 * @brief Serial device configuration.
 *
 * Some OS settings have no equivalent in libhs, and will be set to 0 by hs_serial_get_config().
 * Parameters set to 0 are ignored by hs_serial_set_config().
 *
 * @sa hs_serial_set_config() to change device settings
 * @sa hs_serial_get_config() to get current settings
 */
typedef struct hs_serial_config {
    /** Device baud rate, see @ref hs_serial_rate for accepted values. */
    unsigned int baudrate;

    /** Number of data bits, can be 5, 6, 7 or 8 (or 0 to ignore). */
    unsigned int databits;
    /** Number of stop bits, can be 1 or 2 (or 0 to ignore). */
    unsigned int stopbits;
    /** Serial parity mode. */
    hs_serial_config_parity parity;

    /** RTS pin mode and RTS/CTS flow control. */
    hs_serial_config_rts rts;
    /** DTR pin mode. */
    hs_serial_config_dtr dtr;
    /** Serial XON/XOFF (software) flow control. */
    hs_serial_config_xonxoff xonxoff;
} hs_serial_config;

/**
 * @ingroup serial
 * @brief Set the serial settings associated with a serial device.
 *
 * Each parameter set to 0 will be ignored, and left as is for this device. The following
 * example code will only modify the parity and baudrate settings.
 *
 * @code{.c}
 * hs_serial_config config = {
 *     .baudrate = 115200,
 *     .parity = HS_SERIAL_CONFIG_PARITY_OFF
 * };
 * // This is an example, but you should check for errors
 * hs_serial_set_config(h, &config);
 * @endcode
 *
 * The change is carried out immediately, before the buffers are emptied.
 *
 * @param port   Open serial device handle.
 * @param config Serial settings, see @ref hs_serial_config.
 * @return This function returns 0 on success, or a negative @ref hs_error_code value.
 *
 * @sa hs_serial_config for available serial settings.
 */
int hs_serial_set_config(hs_port *port, const hs_serial_config *config);

/**
 * @ingroup serial
 * @brief Get the serial settings associated with a serial device.
 *
 * Only a subset of parameters available on each OS is recognized. Some hs_serial_config
 * values may be left to 0 if there is no valid libhs equivalent value, such that
 * subsequent hs_serial_set_config() calls should not lose these parameters.
 *
 * You do not need to call hs_serial_get_config() to change only a few settings, see
 * hs_serial_set_config() for more details.
 *
 * @param      port   Open serial device handle.
 * @param[out] config Serial settings, see @ref hs_serial_config.
 * @return This function returns 0 on success, or a negative @ref hs_error_code value.
 *
 * @sa hs_serial_config for available serial settings.
 */
int hs_serial_get_config(hs_port *port, hs_serial_config *config);

/**
 * @ingroup serial
 * @brief Read bytes from a serial device.
 *
 * Read up to @p size bytes from the serial device. If no data is available, the function
 * waits for up to @p timeout milliseconds. Use a negative value to wait indefinitely.
 *
 * @param      port    Device handle.
 * @param[out] buf     Data buffer.
 * @param      size    Size of the buffer.
 * @param      timeout Timeout in milliseconds, or -1 to block indefinitely.
 * @return This function returns the number of bytes read, or a negative @ref hs_error_code value.
 */
ssize_t hs_serial_read(hs_port *port, uint8_t *buf, size_t size, int timeout);
/**
 * @ingroup serial
 * @brief Send bytes to a serial device.
 *
 * Write up to @p size bytes to the device. This is a blocking function, but it may not write
 * all the data passed in.
 *
 * @param port    Device handle.
 * @param buf     Data buffer.
 * @param size    Size of the buffer.
 * @param timeout Timeout in milliseconds, or -1 to block indefinitely.
 * @return This function returns the number of bytes written, or a negative @ref hs_error_code
 *     value.
 */
ssize_t hs_serial_write(hs_port *port, const uint8_t *buf, size_t size, int timeout);

_HS_END_C

#endif


#endif

#ifdef HS_IMPLEMENTATION
    #if defined(_MSC_VER)
        #pragma comment(lib, "user32.lib")
        #pragma comment(lib, "advapi32.lib")
        #pragma comment(lib, "setupapi.lib")
        #pragma comment(lib, "hid.lib")
    #endif

// common_priv.h
// ------------------------------------

#ifndef _HS_COMMON_PRIV_H
#define _HS_COMMON_PRIV_H

// #include "common.h"
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#endif

// device_priv.h
// ------------------------------------

#ifndef _HS_DEVICE_PRIV_H
#define _HS_DEVICE_PRIV_H

// #include "common_priv.h"
// #include "device.h"

struct hs_port {
    hs_device_type type;
    const char *path;
    hs_port_mode mode;
    hs_device *dev;

    union {
#if defined(_WIN32)
        struct {
            void *h; // HANDLE

            struct _OVERLAPPED *read_ov;
            size_t read_buf_size;
            uint8_t *read_buf;
            uint8_t *read_ptr;
            size_t read_len;
            int read_status;

            void *write_event; // HANDLE
        } handle;
#else
        struct {
            int fd;

    #ifdef __linux__
            // Used to work around an old kernel 2.6 (pre-2.6.34) hidraw bug
            uint8_t *read_buf;
            size_t read_buf_size;
            bool numbered_hid_reports;
    #endif
        } file;

    #ifdef __APPLE__
        struct _hs_hid_darwin *hid;
    #endif
#endif
    } u;
};

void _hs_device_log(const hs_device *dev, const char *verb);

int _hs_open_file_port(hs_device *dev, hs_port_mode mode, hs_port **rport);
void _hs_close_file_port(hs_port *port);
hs_handle _hs_get_file_port_poll_handle(const hs_port *port);

#if defined(_WIN32)
void _hs_win32_start_async_read(hs_port *port);
void _hs_win32_finalize_async_read(hs_port *port, int timeout);
ssize_t _hs_win32_write_sync(hs_port *port, const uint8_t *buf, size_t size, int timeout);
#elif defined(__APPLE__)
int _hs_darwin_open_hid_port(hs_device *dev, hs_port_mode mode, hs_port **rport);
void _hs_darwin_close_hid_port(hs_port *port);
hs_handle _hs_darwin_get_hid_port_poll_handle(const hs_port *port);
#endif

#endif

// match_priv.h
// ------------------------------------

#ifndef _HS_MATCH_PRIV_H
#define _HS_MATCH_PRIV_H

// #include "common_priv.h"
// #include "device.h"
// #include "match.h"

typedef struct _hs_match_helper {
    hs_match_spec *specs;
    unsigned int specs_count;

    uint32_t types;
} _hs_match_helper;

int _hs_match_helper_init(_hs_match_helper *helper, const hs_match_spec *specs,
                          unsigned int specs_count);
void _hs_match_helper_release(_hs_match_helper *helper);

bool _hs_match_helper_match(const _hs_match_helper *helper, const hs_device *dev,
                            void **rmatch_udata);
bool _hs_match_helper_has_type(const _hs_match_helper *helper, hs_device_type type);

#endif


// common.c
// ------------------------------------

// #include "common_priv.h"

static hs_log_handler_func *log_handler = hs_log_default_handler;
static void *log_handler_udata;

static _HS_THREAD_LOCAL hs_error_code error_masks[32];
static _HS_THREAD_LOCAL unsigned int error_masks_count;

static _HS_THREAD_LOCAL char last_error_msg[512];

uint32_t hs_version(void)
{
    return HS_VERSION;
}

const char *hs_version_string(void)
{
    return HS_VERSION_STRING;
}

static const char *generic_message(int err)
{
    if (err >= 0)
        return "Success";

    switch ((hs_error_code)err) {
        case HS_ERROR_MEMORY: { return "Memory error"; } break;
        case HS_ERROR_NOT_FOUND: { return "Not found"; } break;
        case HS_ERROR_ACCESS: { return "Permission error"; } break;
        case HS_ERROR_IO: { return "I/O error"; } break;
        case HS_ERROR_PARSE: { return "Parse error"; } break;
        case HS_ERROR_SYSTEM: { return "System error"; } break;
    }

    return "Unknown error";
}

void hs_log_set_handler(hs_log_handler_func *f, void *udata)
{
    assert(f);
    assert(f != hs_log_default_handler || !udata);

    log_handler = f;
    log_handler_udata = udata;
}

void hs_log_default_handler(hs_log_level level, int err, const char *msg, void *udata)
{
    _HS_UNUSED(err);
    _HS_UNUSED(udata);

    if (level == HS_LOG_DEBUG && !getenv("LIBHS_DEBUG"))
        return;

    fputs(msg, stderr);
    fputc('\n', stderr);
}

void hs_error_mask(hs_error_code err)
{
    assert(error_masks_count < _HS_COUNTOF(error_masks));

    error_masks[error_masks_count++] = err;
}

void hs_error_unmask(void)
{
    assert(error_masks_count);

    error_masks_count--;
}

int hs_error_is_masked(int err)
{
    if (err >= 0)
        return 0;

    for (unsigned int i = 0; i < error_masks_count; i++) {
        if (error_masks[i] == err)
            return 1;
    }

    return 0;
}

const char *hs_error_last_message()
{
    return last_error_msg;
}

void hs_log(hs_log_level level, const char *fmt, ...)
{
    assert(fmt);

    va_list ap;
    char buf[sizeof(last_error_msg)];

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    (*log_handler)(level, 0, buf, log_handler_udata);
}

int hs_error(hs_error_code err, const char *fmt, ...)
{
    va_list ap;
    char buf[sizeof(last_error_msg)];

    /* Don't copy directly to last_error_message because we need to support
       ty_error(err, "%s", ty_error_last_message()). */
    if (fmt) {
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
    } else {
        strncpy(buf, generic_message(err), sizeof(buf));
        buf[sizeof(buf) - 1] = 0;
    }

    strcpy(last_error_msg, buf);
    if (!hs_error_is_masked(err))
        (*log_handler)(HS_LOG_ERROR, err, buf, log_handler_udata);

    return err;
}

int _hs_asprintf(char **strp, const char *fmt, ...)
{
    va_list ap;
    int r;

    va_start(ap, fmt);
#ifdef HAVE_ASPRINTF
    r = vasprintf(strp, fmt, ap);
#else
    r = _hs_vasprintf(strp, fmt, ap);
#endif
    va_end(ap);

    return r;
}

int _hs_vasprintf(char **strp, const char *fmt, va_list ap)
{
#ifdef HAVE_ASPRINTF
    return vasprintf(strp, fmt, ap);
#else
    va_list ap_copy;
    char *s;
    int r;

    va_copy(ap_copy, ap);
    r = vsnprintf(NULL, 0, fmt, ap_copy);
    if (r < 0)
        return -1;
    va_end(ap_copy);

    s = (char *)malloc((size_t)r + 1);
    if (!s)
        return -1;

    va_copy(ap_copy, ap);
    r = vsnprintf(s, (size_t)r + 1, fmt, ap_copy);
    if (r < 0)
        return -1;
    va_end(ap_copy);

    *strp = s;
    return r;
#endif
}

// array.c
// ------------------------------------

// #include "common_priv.h"
// #include "array.h"

void _hs_array_release_(struct _hs_array *array)
{
    assert(array);

    free(array->values);

    array->values = NULL;
    array->allocated = 0;
    array->count = 0;
}

int _hs_array_expand_(struct _hs_array *array, size_t value_size, size_t need)
{
    assert(array);
    assert(array->count <= SIZE_MAX - need);

    if (need > array->allocated - array->count) {
        size_t new_size;
        void *new_values;

        new_size = 4;
        while (new_size < array->count)
            new_size += new_size / 2;
        while (need > new_size - array->count)
            new_size += new_size / 2;
        new_values = realloc(array->values, new_size * value_size);
        if (!new_values)
            return hs_error(HS_ERROR_MEMORY, NULL);
        memset((uint8_t *)new_values + (array->allocated * value_size), 0,
               (new_size - array->allocated) * value_size);

        array->values = new_values;
        array->allocated = new_size;
    }

    return 0;
}

void _hs_array_shrink_(struct _hs_array *array, size_t value_size)
{
    assert(array);

    if (array->count) {
        void *new_items = realloc(array->values, array->count * value_size);
        if (!new_items)
            return;

        array->values = new_items;
        array->allocated = array->count;
    } else {
        free(array->values);

        array->values = NULL;
        array->allocated = 0;
    }
}

// device.c
// ------------------------------------

// #include "common_priv.h"
#ifdef _MSC_VER
    #include <windows.h>
#endif
// #include "device_priv.h"
// #include "monitor.h"
// #include "platform.h"

hs_device *hs_device_ref(hs_device *dev)
{
    assert(dev);

#ifdef _MSC_VER
    InterlockedIncrement((long *)&dev->refcount);
#else
    __atomic_fetch_add(&dev->refcount, 1, __ATOMIC_RELAXED);
#endif
    return dev;
}

void hs_device_unref(hs_device *dev)
{
    if (dev) {
#ifdef _MSC_VER
        if (InterlockedDecrement((long *)&dev->refcount))
            return;
#else
        if (__atomic_fetch_sub(&dev->refcount, 1, __ATOMIC_RELEASE) > 1)
            return;
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
#endif

        free(dev->key);
        free(dev->location);
        free(dev->path);

        free(dev->manufacturer_string);
        free(dev->product_string);
        free(dev->serial_number_string);
    }

    free(dev);
}

void _hs_device_log(const hs_device *dev, const char *verb)
{
    switch (dev->type) {
        case HS_DEVICE_TYPE_SERIAL: {
            hs_log(HS_LOG_DEBUG, "%s serial device '%s' on iface %u\n"
                                 "  - USB VID/PID = %04x:%04x, USB location = %s\n"
                                 "  - USB manufacturer = %s, product = %s, S/N = %s",
                   verb, dev->key, dev->iface_number, dev->vid, dev->pid, dev->location,
                   dev->manufacturer_string ? dev->manufacturer_string : "(none)",
                   dev->product_string ? dev->product_string : "(none)",
                   dev->serial_number_string ? dev->serial_number_string : "(none)");
        } break;

        case HS_DEVICE_TYPE_HID: {
            hs_log(HS_LOG_DEBUG, "%s HID device '%s' on iface %u\n"
                                 "  - USB VID/PID = %04x:%04x, USB location = %s\n"
                                 "  - USB manufacturer = %s, product = %s, S/N = %s\n"
                                 "  - HID usage page = 0x%x, HID usage = 0x%x",
                   verb, dev->key, dev->iface_number, dev->vid, dev->pid, dev->location,
                   dev->manufacturer_string ? dev->manufacturer_string : "(none)",
                   dev->product_string ? dev->product_string : "(none)",
                   dev->serial_number_string ? dev->serial_number_string : "(none)",
                   dev->u.hid.usage_page, dev->u.hid.usage);
        } break;
    }
}

int hs_port_open(hs_device *dev, hs_port_mode mode, hs_port **rport)
{
    assert(dev);
    assert(rport);

    if (dev->status != HS_DEVICE_STATUS_ONLINE)
        return hs_error(HS_ERROR_NOT_FOUND, "Device '%s' is not connected", dev->path);

#ifdef __APPLE__
    if (dev->type == HS_DEVICE_TYPE_HID)
        return _hs_darwin_open_hid_port(dev, mode, rport);
#endif

    return _hs_open_file_port(dev, mode, rport);
}

void hs_port_close(hs_port *port)
{
    if (!port)
        return;

#ifdef __APPLE__
    if (port->type == HS_DEVICE_TYPE_HID) {
        _hs_darwin_close_hid_port(port);
        return;
    }
#endif

    _hs_close_file_port(port);
}

hs_device *hs_port_get_device(const hs_port *port)
{
    assert(port);
    return port->dev;
}

hs_handle hs_port_get_poll_handle(const hs_port *port)
{
    assert(port);

#ifdef __APPLE__
    if (port->type == HS_DEVICE_TYPE_HID)
        return _hs_darwin_get_hid_port_poll_handle(port);
#endif

    return _hs_get_file_port_poll_handle(port);
}

// match.c
// ------------------------------------

// #include "common_priv.h"
#ifndef _WIN32
    #include <sys/stat.h>
#endif
// #include "match_priv.h"

int hs_match_parse(const char *str, hs_match_spec *rspec)
{
    unsigned int vid = 0, pid = 0, type = 0;

    str += strspn(str, " ");
    if (str[0]) {
        char type_buf[16];
        int r;

        r = sscanf(str, "%04x:%04x/%15s", &vid, &pid, type_buf);
        if (r < 2)
            return hs_error(HS_ERROR_PARSE, "Malformed device match string '%s'", str);

        if (r == 3) {
            for (unsigned int i = 1; i < _HS_COUNTOF(hs_device_type_strings); i++) {
                if (!strcmp(hs_device_type_strings[i], type_buf))
                    type = i;
            }
            if (!type)
                return hs_error(HS_ERROR_PARSE, "Unknown device type '%s' in match string '%s'",
                                type_buf, str);
        }
    }

    memset(rspec, 0, sizeof(*rspec));
    rspec->vid = (uint16_t)vid;
    rspec->pid = (uint16_t)pid;
    rspec->type = type;
    return 0;
}

int _hs_match_helper_init(_hs_match_helper *helper, const hs_match_spec *specs,
                          unsigned int specs_count)
{
    if (!specs) {
        helper->specs = NULL;
        helper->specs_count = 0;
        helper->types = UINT32_MAX;

        return 0;
    }

    // I promise to be careful
    helper->specs = specs_count ? (hs_match_spec *)specs : NULL;
    helper->specs_count = specs_count;

    helper->types = 0;
    for (unsigned int i = 0; i < specs_count; i++) {
        if (!specs[i].type) {
            helper->types = UINT32_MAX;
            break;
        }

        helper->types |= (uint32_t)(1 << specs[i].type);
    }

    return 0;
}

void _hs_match_helper_release(_hs_match_helper *helper)
{
    _HS_UNUSED(helper);
}

static bool test_spec(const hs_match_spec *spec, const hs_device *dev)
{
    if (spec->type && dev->type != (hs_device_type)spec->type)
        return false;
    if (spec->vid && dev->vid != spec->vid)
        return false;
    if (spec->pid && dev->pid != spec->pid)
        return false;

    return true;
}

bool _hs_match_helper_match(const _hs_match_helper *helper, const hs_device *dev,
                            void **rmatch_udata)
{
    // Do the fast checks first
    if (!_hs_match_helper_has_type(helper, dev->type))
        return false;
    if (!helper->specs_count) {
        if (rmatch_udata)
            *rmatch_udata = NULL;
        return true;
    }

    for (unsigned int i = 0; i < helper->specs_count; i++) {
        if (test_spec(&helper->specs[i], dev)) {
            if (rmatch_udata)
                *rmatch_udata = helper->specs[i].udata;
            return true;
        }
    }

    return false;
}

bool _hs_match_helper_has_type(const _hs_match_helper *helper, hs_device_type type)
{
    return helper->types & (uint32_t)(1 << type);
}

// htable.c
// ------------------------------------

// #include "common_priv.h"
// #include "htable.h"

int _hs_htable_init(_hs_htable *table, unsigned int size)
{
    table->heads = (void **)malloc(size * sizeof(*table->heads));
    if (!table->heads)
        return hs_error(HS_ERROR_MEMORY, NULL);
    table->size = size;

    _hs_htable_clear(table);

    return 0;
}

void _hs_htable_release(_hs_htable *table)
{
    free(table->heads);
}

_hs_htable_head *_hs_htable_get_head(_hs_htable *table, uint32_t key)
{
    return (_hs_htable_head *)&table->heads[key % table->size];
}

void _hs_htable_add(_hs_htable *table, uint32_t key, _hs_htable_head *n)
{
    _hs_htable_head *head = _hs_htable_get_head(table, key);

    n->key = key;

    n->next = head->next;
    head->next = n;
}

void _hs_htable_insert(_hs_htable_head *prev, _hs_htable_head *n)
{
    n->key = prev->key;

    n->next = prev->next;
    prev->next = n;
}

void _hs_htable_remove(_hs_htable_head *head)
{
    for (_hs_htable_head *prev = head->next; prev != head; prev = prev->next) {
        if (prev->next == head) {
            prev->next = head->next;
            head->next = NULL;

            break;
        }
    }
}

void _hs_htable_clear(_hs_htable *table)
{
    for (unsigned int i = 0; i < table->size; i++)
        table->heads[i] = (_hs_htable_head *)&table->heads[i];
}

// monitor_common.c
// ------------------------------------

// #include "common_priv.h"
// #include "device_priv.h"
// #include "match.h"
// #include "monitor_priv.h"

static int find_callback(hs_device *dev, void *udata)
{
    hs_device **rdev = (hs_device **)udata;

    *rdev = hs_device_ref(dev);
    return 1;
}

int hs_find(const hs_match_spec *matches, unsigned int count, hs_device **rdev)
{
    assert(rdev);
    return hs_enumerate(matches, count, find_callback, rdev);
}

void _hs_monitor_clear_devices(_hs_htable *devices)
{
    _hs_htable_foreach(cur, devices) {
        hs_device *dev = _HS_CONTAINER_OF(cur, hs_device, hnode);
        hs_device_unref(dev);
    }
    _hs_htable_clear(devices);
}

bool _hs_monitor_has_device(_hs_htable *devices, const char *key, uint8_t iface)
{
    _hs_htable_foreach_hash(cur, devices, _hs_htable_hash_str(key)) {
        hs_device *dev = _HS_CONTAINER_OF(cur, hs_device, hnode);

        if (strcmp(dev->key, key) == 0 && dev->iface_number == iface)
            return true;
    }

    return false;
}

int _hs_monitor_add(_hs_htable *devices, hs_device *dev, hs_enumerate_func *f, void *udata)
{
    if (_hs_monitor_has_device(devices, dev->key, dev->iface_number))
        return 0;

    hs_device_ref(dev);
    _hs_htable_add(devices, _hs_htable_hash_str(dev->key), &dev->hnode);

    _hs_device_log(dev, "Add");

    if (f) {
        return (*f)(dev, udata);
    } else {
        return 0;
    }
}

void _hs_monitor_remove(_hs_htable *devices, const char *key, hs_enumerate_func *f,
                        void *udata)
{
    _hs_htable_foreach_hash(cur, devices, _hs_htable_hash_str(key)) {
        hs_device *dev = _HS_CONTAINER_OF(cur, hs_device, hnode);

        if (strcmp(dev->key, key) == 0) {
            dev->status = HS_DEVICE_STATUS_DISCONNECTED;

            hs_log(HS_LOG_DEBUG, "Remove device '%s'", dev->key);

            if (f)
                (*f)(dev, udata);

            _hs_htable_remove(&dev->hnode);
            hs_device_unref(dev);
        }
    }
}

int _hs_monitor_list(_hs_htable *devices, hs_enumerate_func *f, void *udata)
{
    _hs_htable_foreach(cur, devices) {
        hs_device *dev = _HS_CONTAINER_OF(cur, hs_device, hnode);
        int r;

        r = (*f)(dev, udata);
        if (r)
            return r;
    }

    return 0;
}

// platform.c
// ------------------------------------

// #include "common_priv.h"
// #include "platform.h"

int hs_adjust_timeout(int timeout, uint64_t start)
{
    if (timeout < 0)
        return -1;

    uint64_t now = hs_millis();

    if (now > start + (uint64_t)timeout)
        return 0;
    return (int)(start + (uint64_t)timeout - now);
}


    #if defined(_WIN32)
// device_win32.c
// ------------------------------------

// #include "common_priv.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
// #include "device_priv.h"
// #include "platform.h"

#define READ_BUFFER_SIZE 16384

int _hs_open_file_port(hs_device *dev, hs_port_mode mode, hs_port **rport)
{
    hs_port *port = NULL;
    DWORD access;
    int r;

    port = (hs_port *)calloc(1, sizeof(*port));
    if (!port) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto error;
    }
    port->type = dev->type;

    port->mode = mode;
    port->path = dev->path;
    port->dev = hs_device_ref(dev);

    access = UINT32_MAX;
    switch (mode) {
        case HS_PORT_MODE_READ: { access = GENERIC_READ; } break;
        case HS_PORT_MODE_WRITE: { access = GENERIC_WRITE; } break;
        case HS_PORT_MODE_RW: { access = GENERIC_READ | GENERIC_WRITE; } break;
    }
    assert(access != UINT32_MAX);

    port->u.handle.h = CreateFileA(dev->path, access, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (port->u.handle.h == INVALID_HANDLE_VALUE) {
        switch (GetLastError()) {
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND: {
                r = hs_error(HS_ERROR_NOT_FOUND, "Device '%s' not found", dev->path);
            } break;
            case ERROR_NOT_ENOUGH_MEMORY:
            case ERROR_OUTOFMEMORY: {
                r = hs_error(HS_ERROR_MEMORY, NULL);
            } break;
            case ERROR_ACCESS_DENIED: {
                r = hs_error(HS_ERROR_ACCESS, "Permission denied for device '%s'", dev->path);
            } break;

            default: {
                r = hs_error(HS_ERROR_SYSTEM, "CreateFile('%s') failed: %s", dev->path,
                             hs_win32_strerror(0));
            } break;
        }
        goto error;
    }

    if (dev->type == HS_DEVICE_TYPE_SERIAL) {
        DCB dcb;
        COMMTIMEOUTS timeouts;
        BOOL success;

        dcb.DCBlength = sizeof(dcb);
        success = GetCommState(port->u.handle.h, &dcb);
        if (!success) {
            r = hs_error(HS_ERROR_SYSTEM, "GetCommState() failed on '%s': %s", port->path,
                         hs_win32_strerror(0));
            goto error;
        }

        /* Sane config, inspired by libserialport, and with DTR pin on by default for
           consistency with UNIX platforms. */
        dcb.fBinary = TRUE;
        dcb.fAbortOnError = FALSE;
        dcb.fErrorChar = FALSE;
        dcb.fNull = FALSE;
        dcb.fDtrControl = DTR_CONTROL_ENABLE;
        dcb.fDsrSensitivity = FALSE;

        /* See SERIAL_TIMEOUTS documentation on MSDN, this basically means "Terminate read request
           when there is at least one byte available". You still need a total timeout in that mode
           so use 0xFFFFFFFE (using 0xFFFFFFFF for all read timeouts is not allowed). Using
           WaitCommEvent() instead would probably be a good idea, I'll look into that later. */
        timeouts.ReadIntervalTimeout = ULONG_MAX;
        timeouts.ReadTotalTimeoutMultiplier = ULONG_MAX;
        timeouts.ReadTotalTimeoutConstant = ULONG_MAX - 1;
        timeouts.WriteTotalTimeoutMultiplier = 0;
        timeouts.WriteTotalTimeoutConstant = 5000;

        success = SetCommState(port->u.handle.h, &dcb);
        if (!success) {
            r = hs_error(HS_ERROR_SYSTEM, "SetCommState() failed on '%s': %s",
                         port->path, hs_win32_strerror(0));
            goto error;
        }
        success = SetCommTimeouts(port->u.handle.h, &timeouts);
        if (!success) {
            r = hs_error(HS_ERROR_SYSTEM, "SetCommTimeouts() failed on '%s': %s",
                         port->path, hs_win32_strerror(0));
            goto error;
        }
        success = PurgeComm(port->u.handle.h, PURGE_RXCLEAR);
        if (!success) {
            r = hs_error(HS_ERROR_SYSTEM, "PurgeComm(PURGE_RXCLEAR) failed on '%s': %s",
                         port->path, hs_win32_strerror(0));
            goto error;
        }
    }

    if (mode & HS_PORT_MODE_READ) {
        port->u.handle.read_ov = (OVERLAPPED *)calloc(1, sizeof(*port->u.handle.read_ov));
        if (!port->u.handle.read_ov) {
            r = hs_error(HS_ERROR_MEMORY, NULL);
            goto error;
        }

        port->u.handle.read_ov->hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!port->u.handle.read_ov->hEvent) {
            r = hs_error(HS_ERROR_SYSTEM, "CreateEvent() failed: %s", hs_win32_strerror(0));
            goto error;
        }

        if (dev->type == HS_DEVICE_TYPE_HID) {
            port->u.handle.read_buf_size = dev->u.hid.input_report_len;
        } else {
            port->u.handle.read_buf_size = READ_BUFFER_SIZE;
        }

        if (port->u.handle.read_buf_size) {
            port->u.handle.read_buf = (uint8_t *)malloc(port->u.handle.read_buf_size);
            if (!port->u.handle.read_buf) {
                r = hs_error(HS_ERROR_MEMORY, NULL);
                goto error;
            }

            _hs_win32_start_async_read(port);
            if (port->u.handle.read_status < 0) {
                r = port->u.handle.read_status;
                goto error;
            }
        }
    }

    if (mode & HS_PORT_MODE_WRITE) {
        port->u.handle.write_event = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!port->u.handle.write_event) {
            r = hs_error(HS_ERROR_SYSTEM, "CreateEvent() failed: %s", hs_win32_strerror(0));
            goto error;
        }
    }

    *rport = port;
    return 0;

error:
    hs_port_close(port);
    return r;
}

void _hs_close_file_port(hs_port *port)
{
    if (port) {
        hs_device_unref(port->dev);
        port->dev = NULL;

        if (CancelIoEx(port->u.handle.h, NULL))
            WaitForSingleObject(port->u.handle.read_ov->hEvent, INFINITE);
        if (port->u.handle.h)
            CloseHandle(port->u.handle.h);

        free(port->u.handle.read_buf);
        if (port->u.handle.read_ov && port->u.handle.read_ov->hEvent)
            CloseHandle(port->u.handle.read_ov->hEvent);
        free(port->u.handle.read_ov);

        if (port->u.handle.write_event)
            CloseHandle(port->u.handle.write_event);
    }

    free(port);
}

hs_handle _hs_get_file_port_poll_handle(const hs_port *port)
{
    return port->u.handle.read_ov->hEvent;
}

// Call only when port->status != 0, otherwise you will leak kernel memory
void _hs_win32_start_async_read(hs_port *port)
{
    DWORD ret;

    ret = (DWORD)ReadFile(port->u.handle.h, port->u.handle.read_buf,
                          (DWORD)port->u.handle.read_buf_size, NULL, port->u.handle.read_ov);
    if (!ret && GetLastError() != ERROR_IO_PENDING) {
        CancelIo(port->u.handle.h);

        port->u.handle.read_status = hs_error(HS_ERROR_IO, "I/O error while reading from '%s'",
                                           port->path);
        return;
    }

    port->u.handle.read_status = 0;
}

void _hs_win32_finalize_async_read(hs_port *port, int timeout)
{
    DWORD len, ret;

    if (!port->u.handle.read_buf_size)
        return;

    if (timeout > 0)
        WaitForSingleObject(port->u.handle.read_ov->hEvent, (DWORD)timeout);
    ret = (DWORD)GetOverlappedResult(port->u.handle.h, port->u.handle.read_ov, &len, timeout < 0);
    if (!ret) {
        if (GetLastError() == ERROR_IO_INCOMPLETE) {
            port->u.handle.read_status = 0;
            return;
        }

        port->u.handle.read_status = hs_error(HS_ERROR_IO, "I/O error while reading from '%s'",
                                              port->path);
        return;
    }

    port->u.handle.read_len = (size_t)len;
    port->u.handle.read_ptr = port->u.handle.read_buf;

    port->u.handle.read_status = 1;
}

ssize_t _hs_win32_write_sync(hs_port *port, const uint8_t *buf, size_t size, int timeout)
{
    OVERLAPPED ov = {0};
    DWORD len;
    BOOL success;

    ov.hEvent = port->u.handle.write_event;
    success = WriteFile(port->u.handle.h, buf, (DWORD)size, NULL, &ov);
    if (!success && GetLastError() != ERROR_IO_PENDING)
        return hs_error(HS_ERROR_IO, "I/O error while writing to '%s'", port->path);

    if (timeout > 0)
        WaitForSingleObject(ov.hEvent, (DWORD)timeout);

    success = GetOverlappedResult(port->u.handle.h, &ov, &len, timeout < 0);
    if (!success) {
        if (GetLastError() == ERROR_IO_INCOMPLETE) {
            CancelIoEx(port->u.handle.h, &ov);

            success = GetOverlappedResult(port->u.handle.h, &ov, &len, TRUE);
            if (!success)
                len = 0;
        } else {
            return hs_error(HS_ERROR_IO, "I/O error while writing to '%s'", port->path);
        }
    }

    return (ssize_t)len;
}

// hid_win32.c
// ------------------------------------

// #include "common_priv.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
_HS_BEGIN_C
    #include <hidsdi.h>
_HS_END_C
#include <winioctl.h>
// #include "device_priv.h"
// #include "hid.h"
// #include "platform.h"

// Copied from hidclass.h in the MinGW-w64 headers
#define HID_OUT_CTL_CODE(id) \
    CTL_CODE(FILE_DEVICE_KEYBOARD, (id), METHOD_OUT_DIRECT, FILE_ANY_ACCESS)
#define IOCTL_HID_GET_FEATURE HID_OUT_CTL_CODE(100)

ssize_t hs_hid_read(hs_port *port, uint8_t *buf, size_t size, int timeout)
{
    assert(port);
    assert(port->dev->type == HS_DEVICE_TYPE_HID);
    assert(port->mode & HS_PORT_MODE_READ);
    assert(buf);
    assert(size);

    if (port->u.handle.read_status < 0) {
        // Could be a transient error, try to restart it
        _hs_win32_start_async_read(port);
        if (port->u.handle.read_status < 0)
            return port->u.handle.read_status;
    }

    _hs_win32_finalize_async_read(port, timeout);
    if (port->u.handle.read_status <= 0)
        return port->u.handle.read_status;

    /* HID communication is message-based. So if the caller does not provide a big enough
       buffer, we can just discard the extra data, unlike for serial communication. */
    if (port->u.handle.read_len) {
        if (size > port->u.handle.read_len)
            size = port->u.handle.read_len;
        memcpy(buf, port->u.handle.read_buf, size);
    } else {
        size = 0;
    }

    hs_error_mask(HS_ERROR_IO);
    _hs_win32_start_async_read(port);
    hs_error_unmask();

    return (ssize_t)size;
}

ssize_t hs_hid_write(hs_port *port, const uint8_t *buf, size_t size)
{
    assert(port);
    assert(port->dev->type == HS_DEVICE_TYPE_HID);
    assert(port->mode & HS_PORT_MODE_WRITE);
    assert(buf);

    if (size < 2)
        return 0;

    ssize_t r = _hs_win32_write_sync(port, buf, size, 5000);
    if (!r)
        return hs_error(HS_ERROR_IO, "Timed out while writing to '%s'", port->dev->path);

    return r;
}

ssize_t hs_hid_get_feature_report(hs_port *port, uint8_t report_id, uint8_t *buf, size_t size)
{
    assert(port);
    assert(port->dev->type == HS_DEVICE_TYPE_HID);
    assert(port->mode & HS_PORT_MODE_READ);
    assert(buf);
    assert(size);

    OVERLAPPED ov = {0};
    DWORD len;
    BOOL success;

    buf[0] = report_id;
    len = (DWORD)size;

    success = DeviceIoControl(port->u.handle.h, IOCTL_HID_GET_FEATURE, buf, (DWORD)size, buf,
                              (DWORD)size, NULL, &ov);
    if (!success && GetLastError() != ERROR_IO_PENDING) {
        CancelIo(port->u.handle.h);
        return hs_error(HS_ERROR_IO, "I/O error while writing to '%s'", port->dev->path);
    }

    success = GetOverlappedResult(port->u.handle.h, &ov, &len, TRUE);
    if (!success)
        return hs_error(HS_ERROR_IO, "I/O error while writing to '%s'", port->dev->path);

    /* Apparently the length returned by the IOCTL_HID_GET_FEATURE ioctl does not account
       for the report ID byte. */
    return (ssize_t)len + 1;
}

ssize_t hs_hid_send_feature_report(hs_port *port, const uint8_t *buf, size_t size)
{
    assert(port);
    assert(port->dev->type == HS_DEVICE_TYPE_HID);
    assert(port->mode & HS_PORT_MODE_WRITE);
    assert(buf);

    if (size < 2)
        return 0;

    // Timeout behavior?
    BOOL success = HidD_SetFeature(port->u.handle.h, (char *)buf, (DWORD)size);
    if (!success)
        return hs_error(HS_ERROR_IO, "I/O error while writing to '%s'", port->dev->path);

    return (ssize_t)size;
}

// monitor_win32.c
// ------------------------------------

// #include "common_priv.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cfgmgr32.h>
#include <dbt.h>
#include <devioctl.h>
_HS_BEGIN_C
    #include <hidsdi.h>
    #include <hidpi.h>
_HS_END_C
#include <initguid.h>
#include <process.h>
#include <setupapi.h>
#include <usb.h>
#include <usbiodef.h>
#include <usbioctl.h>
#include <usbuser.h>
#include <wchar.h>
// #include "array.h"
// #include "device_priv.h"
// #include "match_priv.h"
// #include "monitor_priv.h"
// #include "platform.h"

enum event_type {
    DEVICE_EVENT_ADDED,
    DEVICE_EVENT_REMOVED
};

struct event {
    enum event_type type;
    char device_key[256];
};

typedef _HS_ARRAY(struct event) event_array;

struct hs_monitor {
    _hs_match_helper match_helper;
    _hs_htable devices;

    HANDLE thread;
    HWND thread_hwnd;

    HANDLE thread_event;
    CRITICAL_SECTION events_lock;
    event_array events;
    event_array thread_events;
    event_array refresh_events;
    int thread_ret;
};

struct setup_class {
    const char *name;
    hs_device_type type;
};

struct device_cursor {
    DEVINST inst;
    char id[256];
};

enum device_cursor_relative {
    DEVINST_RELATIVE_PARENT,
    DEVINST_RELATIVE_SIBLING,
    DEVINST_RELATIVE_CHILD
};

#if defined(__MINGW64_VERSION_MAJOR) && __MINGW64_VERSION_MAJOR < 4
__declspec(dllimport) BOOLEAN NTAPI HidD_GetSerialNumberString(HANDLE HidDeviceObject,
                                                               PVOID Buffer, ULONG BufferLength);
__declspec(dllimport) BOOLEAN NTAPI HidD_GetPreparsedData(HANDLE HidDeviceObject,
                                                          PHIDP_PREPARSED_DATA *PreparsedData);
__declspec(dllimport) BOOLEAN NTAPI HidD_FreePreparsedData(PHIDP_PREPARSED_DATA PreparsedData);
#endif

#define MAX_USB_DEPTH 8
#define MONITOR_CLASS_NAME "hs_monitor"

static const struct setup_class setup_classes[] = {
    {"Ports",    HS_DEVICE_TYPE_SERIAL},
    {"HIDClass", HS_DEVICE_TYPE_HID}
};

static volatile LONG controllers_lock_setup;
static CRITICAL_SECTION controllers_lock;
static char *controllers[32];
static unsigned int controllers_count;

static bool make_device_cursor(DEVINST inst, struct device_cursor *new_cursor)
{
    CONFIGRET cret;

    new_cursor->inst = inst;
    cret = CM_Get_Device_IDA(inst, new_cursor->id, sizeof(new_cursor->id), 0);
    if (cret != CR_SUCCESS) {
        hs_log(HS_LOG_WARNING, "CM_Get_Device_ID() failed for instance 0x%lx: 0x%lx", inst, cret);
        return false;
    }

    return true;
}

static bool make_relative_cursor(struct device_cursor *cursor,
                                 enum device_cursor_relative relative,
                                 struct device_cursor *new_cursor)
{
    DEVINST new_inst;
    CONFIGRET cret = 0xFFFFFFFF;

    switch (relative) {
        case DEVINST_RELATIVE_PARENT: {
            cret = CM_Get_Parent(&new_inst, cursor->inst, 0);
            if (cret != CR_SUCCESS) {
                hs_log(HS_LOG_DEBUG, "Cannot get parent of device '%s': 0x%lx", cursor->id, cret);
                return false;
            }
        } break;

        case DEVINST_RELATIVE_CHILD: {
            cret = CM_Get_Child(&new_inst, cursor->inst, 0);
            if (cret != CR_SUCCESS) {
                hs_log(HS_LOG_DEBUG, "Cannot get child of device '%s': 0x%lx", cursor->id, cret);
                return false;
            }
        } break;

        case DEVINST_RELATIVE_SIBLING: {
            cret = CM_Get_Sibling(&new_inst, cursor->inst, 0);
            if (cret != CR_SUCCESS) {
                hs_log(HS_LOG_DEBUG, "Cannot get sibling of device '%s': 0x%lx", cursor->id, cret);
                return false;
            }
        } break;
    }
    assert(cret != 0xFFFFFFFF);

    return make_device_cursor(new_inst, new_cursor);
}

static uint8_t find_controller(const char *id)
{
    for (unsigned int i = 0; i < controllers_count; i++) {
        if (strcmp(controllers[i], id) == 0)
            return (uint8_t)(i + 1);
    }

    return 0;
}

static int build_device_path(const char *id, const GUID *guid, char **rpath)
{
    char *path, *ptr;

    path = (char *)malloc(4 + strlen(id) + 41);
    if (!path)
        return hs_error(HS_ERROR_MEMORY, NULL);

    strcpy(path, "\\\\.\\");
    ptr = path + 4;

    while (*id) {
        if (*id == '\\') {
            *ptr++ = '#';
            id++;
        } else {
            *ptr++ = *id++;
        }
    }

    sprintf(ptr, "#{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
            guid->Data1, guid->Data2, guid->Data3, guid->Data4[0], guid->Data4[1],
            guid->Data4[2], guid->Data4[3], guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);

    *rpath = path;
    return 0;
}

static int build_location_string(uint8_t ports[], unsigned int depth, char **rpath)
{
    char buf[256];
    char *ptr;
    size_t size;
    char *path;
    int r;

    ptr = buf;
    size = sizeof(buf);

    ptr = strcpy(buf, "usb");
    ptr = buf + 3;
    size -= (size_t)(ptr - buf);

    for (size_t i = 0; i < depth; i++) {
        r = snprintf(ptr, size, "-%hhu", ports[i]);
        assert(r >= 2 && (size_t)r < size);

        ptr += r;
        size -= (size_t)r;
    }

    path = strdup(buf);
    if (!path)
        return hs_error(HS_ERROR_MEMORY, NULL);

    *rpath = path;
    return 0;
}

static int wide_to_cstring(const wchar_t *wide, size_t size, char **rs)
{
    wchar_t *tmp = NULL;
    char *s = NULL;
    int len, r;

    tmp = (wchar_t *)calloc(1, size + sizeof(wchar_t));
    if (!tmp) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto cleanup;
    }

    memcpy(tmp, wide, size);

    len = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, tmp, -1, NULL, 0, NULL, NULL);
    if (!len) {
        r = hs_error(HS_ERROR_SYSTEM, "Failed to convert UTF-16 string to local codepage: %s",
                     hs_win32_strerror(0));
        goto cleanup;
    }

    s = (char *)malloc((size_t)len);
    if (!s) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto cleanup;
    }

    len = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, tmp, -1, s, len, NULL, NULL);
    if (!len) {
        r = hs_error(HS_ERROR_SYSTEM, "Failed to convert UTF-16 string to local codepage: %s",
                     hs_win32_strerror(0));
        goto cleanup;
    }

    *rs = s;
    s = NULL;

    r = 0;
cleanup:
    free(s);
    free(tmp);
    return r;
}

static int get_port_driverkey(HANDLE hub, uint8_t port, char **rkey)
{
    DWORD len;
    USB_NODE_CONNECTION_INFORMATION_EX *node;
    USB_NODE_CONNECTION_DRIVERKEY_NAME pseudo = {0};
    USB_NODE_CONNECTION_DRIVERKEY_NAME *wide = NULL;
    BOOL success;
    int r;

    len = sizeof(node) + (sizeof(USB_PIPE_INFO) * 30);
    node = (USB_NODE_CONNECTION_INFORMATION_EX *)calloc(1, len);
    if (!node) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto cleanup;
    }

    node->ConnectionIndex = port;
    pseudo.ConnectionIndex = port;

    success = DeviceIoControl(hub, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX, node, len,
                              node, len, &len, NULL);
    if (!success) {
        hs_log(HS_LOG_WARNING, "DeviceIoControl(IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX) failed");
        r = 0;
        goto cleanup;
    }

    if (node->ConnectionStatus != DeviceConnected) {
        r = 0;
        goto cleanup;
    }

    success = DeviceIoControl(hub, IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME, &pseudo, sizeof(pseudo),
                              &pseudo, sizeof(pseudo), &len, NULL);
    if (!success) {
        hs_log(HS_LOG_WARNING, "DeviceIoControl(IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME) failed");
        r = 0;
        goto cleanup;
    }

    wide = (USB_NODE_CONNECTION_DRIVERKEY_NAME *)calloc(1, pseudo.ActualLength);
    if (!wide) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto cleanup;
    }

    wide->ConnectionIndex = port;

    success = DeviceIoControl(hub, IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME, wide, pseudo.ActualLength,
                              wide, pseudo.ActualLength, &len, NULL);
    if (!success) {
        hs_log(HS_LOG_WARNING, "DeviceIoControl(IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME) failed");
        r = 0;
        goto cleanup;
    }

    r = wide_to_cstring(wide->DriverKeyName, len - sizeof(pseudo) + 1, rkey);
    if (r < 0)
        goto cleanup;

    r = 1;
cleanup:
    free(wide);
    free(node);
    return r;
}

static int find_device_port_ioctl(const char *hub_id, const char *child_key)
{
    char *path = NULL;
    HANDLE h = NULL;
    USB_NODE_INFORMATION node;
    DWORD len;
    BOOL success;
    int r;

    r = build_device_path(hub_id, &GUID_DEVINTERFACE_USB_HUB, &path);
    if (r < 0)
        goto cleanup;

    h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        hs_log(HS_LOG_DEBUG, "Failed to open USB hub '%s': %s", path, hs_win32_strerror(0));
        r = 0;
        goto cleanup;
    }

    hs_log(HS_LOG_DEBUG, "Asking HUB at '%s' for port information (legacy code path)", path);
    success = DeviceIoControl(h, IOCTL_USB_GET_NODE_INFORMATION, NULL, 0, &node, sizeof(node),
                              &len, NULL);
    if (!success) {
        hs_log(HS_LOG_DEBUG, "DeviceIoControl(IOCTL_USB_GET_NODE_INFORMATION) failed");
        r = 0;
        goto cleanup;
    }

    for (uint8_t port = 1; port <= node.u.HubInformation.HubDescriptor.bNumberOfPorts; port++) {
        char *key = NULL;

        r = get_port_driverkey(h, port, &key);
        if (r < 0)
            goto cleanup;
        if (!r)
            continue;

        if (strcmp(key, child_key) == 0) {
            free(key);

            r = port;
            break;
        } else {
            free(key);
        }
    }

cleanup:
    if (h)
        CloseHandle(h);
    free(path);
    return r;
}

static bool is_root_usb_controller(const char *id)
{
    static const char *const root_needles[] = {
        "ROOT_HUB",
        "VMUSB\\HUB" // Microsoft Virtual PC
    };

    for (size_t i = 0; i < _HS_COUNTOF(root_needles); i++) {
        if (strstr(id, root_needles[i]))
            return true;
    }
    return false;
}

static int resolve_usb_location_ioctl(struct device_cursor usb_cursor, uint8_t ports[],
                                      struct device_cursor *roothub_cursor)
{
    unsigned int depth;

    depth = 0;
    do {
        struct device_cursor parent_cursor;
        char child_key[256];
        DWORD child_key_len;
        CONFIGRET cret;
        int r;

        if (!make_relative_cursor(&usb_cursor, DEVINST_RELATIVE_PARENT, &parent_cursor))
            return 0;

        child_key_len = sizeof(child_key);
        cret = CM_Get_DevNode_Registry_PropertyA(usb_cursor.inst, CM_DRP_DRIVER, NULL,
                                                 child_key, &child_key_len, 0);
        if (cret != CR_SUCCESS) {
            hs_log(HS_LOG_WARNING, "Failed to get device driver key: 0x%lx", cret);
            return 0;
        }
        r = find_device_port_ioctl(parent_cursor.id, child_key);
        if (r <= 0)
            return r;
        ports[depth] = (uint8_t)r;
        hs_log(HS_LOG_DEBUG, "Found port number of '%s': %u", usb_cursor.id, ports[depth]);
        depth++;

        // We need place for the root hub index
        if (depth == MAX_USB_DEPTH) {
            hs_log(HS_LOG_WARNING, "Excessive USB location depth, ignoring device");
            return 0;
        }

        usb_cursor = parent_cursor;
    } while (!is_root_usb_controller(usb_cursor.id));

    *roothub_cursor = usb_cursor;
    return (int)depth;
}

static int resolve_usb_location_cfgmgr(struct device_cursor usb_cursor, uint8_t ports[],
                                       struct device_cursor *roothub_cursor)
{
    unsigned int depth;

    depth = 0;
    do {
        char location_buf[256];
        DWORD location_len;
        unsigned int location_port;
        CONFIGRET cret;

        // Extract port from CM_DRP_LOCATION_INFORMATION (Vista and later versions)
        location_len = sizeof(location_buf);
        cret = CM_Get_DevNode_Registry_PropertyA(usb_cursor.inst, CM_DRP_LOCATION_INFORMATION,
                                                 NULL, location_buf, &location_len, 0);
        if (cret != CR_SUCCESS) {
            hs_log(HS_LOG_DEBUG, "No location information on this device node");
            return 0;
        }
        location_port = 0;
        sscanf(location_buf, "Port_#%04u", &location_port);
        if (!location_port)
            return 0;
        hs_log(HS_LOG_DEBUG, "Found port number of '%s': %u", usb_cursor.id, location_port);
        ports[depth++] = (uint8_t)location_port;

        // We need place for the root hub index
        if (depth == MAX_USB_DEPTH) {
            hs_log(HS_LOG_WARNING, "Excessive USB location depth, ignoring device");
            return 0;
        }

        if (!make_relative_cursor(&usb_cursor, DEVINST_RELATIVE_PARENT, &usb_cursor))
            return 0;
    } while (!is_root_usb_controller(usb_cursor.id));

    *roothub_cursor = usb_cursor;
    return (int)depth;
}

static int find_device_location(const struct device_cursor *dev_cursor, uint8_t ports[])
{
    struct device_cursor usb_cursor;
    struct device_cursor roothub_cursor;
    int depth;

    // Find the USB device instance
    usb_cursor = *dev_cursor;
    while (strncmp(usb_cursor.id, "USB\\", 4) != 0 || strstr(usb_cursor.id, "&MI_")) {
        if (!make_relative_cursor(&usb_cursor, DEVINST_RELATIVE_PARENT, &usb_cursor))
            return 0;
    }

    /* Browse the USB tree to resolve USB ports. Try the CfgMgr method first, only
       available on Windows Vista and later versions. It may also fail with third-party
       USB controller drivers, typically USB 3.0 host controllers before Windows 10. */
    depth = 0;
    if (!getenv("LIBHS_WIN32_FORCE_XP_LOCATION_CODE")) {
        depth = resolve_usb_location_cfgmgr(usb_cursor, ports, &roothub_cursor);
        if (depth < 0)
            return depth;
    }
    if (!depth) {
        hs_log(HS_LOG_DEBUG, "Using legacy code for location of '%s'", dev_cursor->id);
        depth = resolve_usb_location_ioctl(usb_cursor, ports, &roothub_cursor);
        if (depth < 0)
            return depth;
    }
    if (!depth) {
        hs_log(HS_LOG_DEBUG, "Cannot resolve USB location for '%s'", dev_cursor->id);
        return 0;
    }

    // Resolve the USB controller ID
    ports[depth] = find_controller(roothub_cursor.id);
    if (!ports[depth]) {
        hs_log(HS_LOG_WARNING, "Unknown USB host controller '%s'", roothub_cursor.id);
        return 0;
    }
    hs_log(HS_LOG_DEBUG, "Found controller ID for '%s': %u", roothub_cursor.id, ports[depth]);
    depth++;

    // The ports are in the wrong order
    for (int i = 0; i < depth / 2; i++) {
        uint8_t tmp = ports[i];
        ports[i] = ports[depth - i - 1];
        ports[depth - i - 1] = tmp;
    }

    return depth;
}

static int read_hid_properties(hs_device *dev, const USB_DEVICE_DESCRIPTOR *desc)
{
    HANDLE h = NULL;
    int r;

    h = CreateFileA(dev->path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        hs_log(HS_LOG_WARNING, "Cannot open HID device '%s': %s", dev->path, hs_win32_strerror(0));
        r = 0;
        goto cleanup;
    }

#define READ_HID_PROPERTY(index, func, dest) \
        if (index) { \
            wchar_t wbuf[256]; \
            BOOL bret; \
            \
            bret = func(h, wbuf, sizeof(wbuf)); \
            if (bret) { \
                wbuf[_HS_COUNTOF(wbuf) - 1] = 0; \
                r = wide_to_cstring(wbuf, wcslen(wbuf) * sizeof(wchar_t), (dest)); \
                if (r < 0) \
                    goto cleanup; \
            } else { \
                hs_log(HS_LOG_WARNING, "Function %s() failed despite non-zero string index", #func); \
            } \
        }

    READ_HID_PROPERTY(desc->iManufacturer, HidD_GetManufacturerString, &dev->manufacturer_string);
    READ_HID_PROPERTY(desc->iProduct, HidD_GetProductString, &dev->product_string);
    READ_HID_PROPERTY(desc->iSerialNumber, HidD_GetSerialNumberString, &dev->serial_number_string);

#undef READ_HID_PROPERTY

    {
        // semi-hidden Hungarian pointers? Really , Microsoft?
        PHIDP_PREPARSED_DATA pp;
        HIDP_CAPS caps;
        LONG lret;

        lret = HidD_GetPreparsedData(h, &pp);
        if (!lret) {
            hs_log(HS_LOG_WARNING, "HidD_GetPreparsedData() failed on '%s", dev->path);
            r = 0;
            goto cleanup;
        }
        lret = HidP_GetCaps(pp, &caps);
        HidD_FreePreparsedData(pp);
        if (lret != HIDP_STATUS_SUCCESS) {
            hs_log(HS_LOG_WARNING, "Invalid HID descriptor from '%s", dev->path);
            r = 0;
            goto cleanup;
        }

        dev->u.hid.usage_page = caps.UsagePage;
        dev->u.hid.usage = caps.Usage;
        dev->u.hid.input_report_len = caps.InputReportByteLength;
    }

    r = 1;
cleanup:
    if (h)
        CloseHandle(h);
    return r;
}

static int get_string_descriptor(HANDLE hub, uint8_t port, uint8_t index, char **rs)
{
    // A bit ugly, but using USB_DESCRIPTOR_REQUEST directly triggers a C2229 on MSVC
    struct {
        // USB_DESCRIPTOR_REQUEST
        struct {
            ULONG  ConnectionIndex;
            struct {
                UCHAR bmRequest;
                UCHAR bRequest;
                USHORT wValue;
                USHORT wIndex;
                USHORT wLength;
            } SetupPacket;
        } req;

        // Filled by DeviceIoControl
        struct {
            UCHAR bLength;
            UCHAR bDescriptorType;
            WCHAR bString[MAXIMUM_USB_STRING_LENGTH];
        } desc;
    } rq;
    DWORD desc_len = 0;
    char *s;
    BOOL success;
    int r;

    memset(&rq, 0, sizeof(rq));
    rq.req.ConnectionIndex = port;
    rq.req.SetupPacket.wValue = (USHORT)((USB_STRING_DESCRIPTOR_TYPE << 8) | index);
    rq.req.SetupPacket.wIndex = 0x409;
    rq.req.SetupPacket.wLength = sizeof(rq.desc);

    success = DeviceIoControl(hub, IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION, &rq,
                              sizeof(rq), &rq, sizeof(rq), &desc_len, NULL);
    if (!success || desc_len < 2 || rq.desc.bDescriptorType != USB_STRING_DESCRIPTOR_TYPE ||
            rq.desc.bLength != desc_len - sizeof(rq.req) || rq.desc.bLength % 2 != 0) {
        hs_log(HS_LOG_DEBUG, "Invalid string descriptor %u", index);
        return 0;
    }

    r = wide_to_cstring(rq.desc.bString, desc_len - sizeof(USB_DESCRIPTOR_REQUEST), &s);
    if (r < 0)
        return r;

    *rs = s;
    return 0;
}

static int read_device_properties(hs_device *dev, const struct device_cursor *dev_cursor, uint8_t port)
{
    struct device_cursor intf_cursor, usb_cursor, hub_cursor;
    unsigned int vid, pid, iface_number;
    char *path = NULL;
    HANDLE hub = NULL;
    DWORD len;
    USB_NODE_CONNECTION_INFORMATION_EX *node = NULL;
    BOOL success;
    int r;

    // Find relevant device instances: interface, device, HUB
    intf_cursor = *dev_cursor;
    while (strncmp(intf_cursor.id, "USB\\", 4) != 0) {
        if (!make_relative_cursor(&intf_cursor, DEVINST_RELATIVE_PARENT, &intf_cursor)) {
            r = 0;
            goto cleanup;
        }
    }
    if (strstr(intf_cursor.id, "&MI_")) {
        if (!make_relative_cursor(&intf_cursor, DEVINST_RELATIVE_PARENT, &usb_cursor)) {
            r = 0;
            goto cleanup;
        }
    } else {
        usb_cursor = intf_cursor;
    }
    if (!make_relative_cursor(&usb_cursor, DEVINST_RELATIVE_PARENT, &hub_cursor)) {
        r = 0;
        goto cleanup;
    }

    // Make device key (used for removal, among other things)
    dev->key = strdup(usb_cursor.id);
    if (!dev->key) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto cleanup;
    }

    /* h and hh type modifier characters are not known to msvcrt, and MinGW issues warnings
       if we try to use them. Use temporary unsigned int variables to get around that. */
    iface_number = 0;
    r = sscanf(intf_cursor.id, "USB\\VID_%04x&PID_%04x&MI_%02u", &vid, &pid, &iface_number);
    if (r < 2) {
        hs_log(HS_LOG_WARNING, "Failed to parse USB properties from '%s'", intf_cursor.id);
        r = 0;
        goto cleanup;
    }
    dev->vid = (uint16_t)vid;
    dev->pid = (uint16_t)pid;
    dev->iface_number = (uint8_t)iface_number;

    r = build_device_path(hub_cursor.id, &GUID_DEVINTERFACE_USB_HUB, &path);
    if (r < 0)
        goto cleanup;

    hub = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hub == INVALID_HANDLE_VALUE) {
        hs_log(HS_LOG_DEBUG, "Cannot open parent hub device at '%s', ignoring device properties for '%s'",
               path, dev_cursor->id);
        r = 1;
        goto cleanup;
    }

    len = sizeof(node) + (sizeof(USB_PIPE_INFO) * 30);
    node = (USB_NODE_CONNECTION_INFORMATION_EX *)calloc(1, len);
    if (!node) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto cleanup;
    }

    node->ConnectionIndex = port;
    success = DeviceIoControl(hub, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX, node, len,
                              node, len, &len, NULL);
    if (!success) {
        hs_log(HS_LOG_DEBUG, "Failed to interrogate hub device at '%s' for device '%s'",
               path, dev_cursor->id);
        r = 1;
        goto cleanup;
    }

    // Get additional properties
    dev->bcd_device = node->DeviceDescriptor.bcdDevice;

    /* Descriptor requests to USB devices underlying HID devices fail most (all?) of the time,
       so we need a different technique here. We still need the device descriptor because the
       HidD_GetXString() functions sometime return garbage (at least on XP) when the string
       index is 0. */
    if (dev->type == HS_DEVICE_TYPE_HID) {
        r = read_hid_properties(dev, &node->DeviceDescriptor);
        goto cleanup;
    }

#define READ_STRING_DESCRIPTOR(index, var) \
        if (index) { \
            r = get_string_descriptor(hub, port, (index), (var)); \
            if (r < 0) \
                goto cleanup; \
        }

    READ_STRING_DESCRIPTOR(node->DeviceDescriptor.iManufacturer, &dev->manufacturer_string);
    READ_STRING_DESCRIPTOR(node->DeviceDescriptor.iProduct, &dev->product_string);
    READ_STRING_DESCRIPTOR(node->DeviceDescriptor.iSerialNumber, &dev->serial_number_string);

#undef READ_STRING_DESCRIPTOR

    r = 1;
cleanup:
    free(node);
    if (hub)
        CloseHandle(hub);
    free(path);
    return r;
}

static int get_device_comport(DEVINST inst, char **rnode)
{
    HKEY key;
    char buf[32];
    DWORD len;
    char *node;
    CONFIGRET cret;
    LONG ret;
    int r;

    cret = CM_Open_DevNode_Key(inst, KEY_READ, 0, RegDisposition_OpenExisting, &key, CM_REGISTRY_HARDWARE);
    if (cret != CR_SUCCESS) {
        hs_log(HS_LOG_WARNING, "CM_Open_DevNode_Key() failed: 0x%lu", cret);
        return 0;
    }

    len = (DWORD)sizeof(buf);
    ret = RegGetValueA(key, "", "PortName", RRF_RT_REG_SZ, NULL, (BYTE *)buf, &len);
    RegCloseKey(key);
    if (ret != ERROR_SUCCESS) {
        if (ret != ERROR_FILE_NOT_FOUND)
            hs_log(HS_LOG_WARNING, "RegQueryValue() failed: %ld", ret);
        return 0;
    }
    len--;

    // You need the \\.\ prefix to open COM ports beyond COM9
    r = _hs_asprintf(&node, "%s%s", len > 4 ? "\\\\.\\" : "", buf);
    if (r < 0)
        return hs_error(HS_ERROR_MEMORY, NULL);

    *rnode = node;
    return 1;
}

static int find_device_node(hs_device *dev, const struct device_cursor *dev_cursor)
{
    int r;

    /* GUID_DEVINTERFACE_COMPORT only works for real COM ports... Haven't found any way to
       list virtual (USB) serial device interfaces, so instead list USB devices and consider
       them serial if registry key "PortName" is available (and use its value as device node). */
    if (strncmp(dev_cursor->id, "USB\\", 4) == 0 || strncmp(dev_cursor->id, "FTDIBUS\\", 8) == 0) {
        r = get_device_comport(dev_cursor->inst, &dev->path);
        if (!r) {
            hs_log(HS_LOG_DEBUG, "Device '%s' has no 'PortName' registry property", dev_cursor->id);
            return r;
        }
        if (r < 0)
            return r;

        dev->type = HS_DEVICE_TYPE_SERIAL;
    } else if (strncmp(dev_cursor->id, "HID\\", 4) == 0) {
        static GUID hid_interface_guid;
        if (!hid_interface_guid.Data1)
            HidD_GetHidGuid(&hid_interface_guid);

        r = build_device_path(dev_cursor->id, &hid_interface_guid, &dev->path);
        if (r < 0)
            return r;

        dev->type = HS_DEVICE_TYPE_HID;
    } else {
        hs_log(HS_LOG_DEBUG, "Unknown device type for '%s'", dev_cursor->id);
        return 0;
    }

    return 1;
}

static int process_win32_device(DEVINST inst, hs_device **rdev)
{
    struct device_cursor dev_cursor;
    hs_device *dev = NULL;
    uint8_t ports[MAX_USB_DEPTH];
    unsigned int depth;
    int r;

    if (!make_device_cursor(inst, &dev_cursor)) {
        r = 0;
        goto cleanup;
    }

    dev = (hs_device *)calloc(1, sizeof(*dev));
    if (!dev) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto cleanup;
    }
    dev->refcount = 1;
    dev->status = HS_DEVICE_STATUS_ONLINE;

    // HID devices can have multiple collections for each interface, ignore them
    if (strncmp(dev_cursor.id, "HID\\", 4) == 0) {
        const char *ptr = strstr(dev_cursor.id, "&COL");
        if (ptr && strncmp(ptr, "&COL01\\",  7) != 0) {
            hs_log(HS_LOG_DEBUG, "Ignoring duplicate HID collection device '%s'", dev_cursor.id);
            r = 0;
            goto cleanup;
        }
    }

    hs_log(HS_LOG_DEBUG, "Examining device node '%s'", dev_cursor.id);

    // Ignore composite devices
    {
        char class[512];
        DWORD class_len;
        CONFIGRET cret;

        class_len = sizeof(class);
        cret = CM_Get_DevNode_Registry_PropertyA(inst, CM_DRP_CLASSGUID, NULL, class, &class_len, 0);
        if (cret != CR_SUCCESS) {
            hs_log(HS_LOG_WARNING, "Failed to get device class GUID: 0x%lx", cret);
            r = 0;
            goto cleanup;
        }

        if (!strcasecmp(class, "{36fc9e60-c465-11cf-8056-444553540000}")) {
            hs_log(HS_LOG_DEBUG, "Ignoring composite device");
            r = 0;
            goto cleanup;
        }
    }

    r = find_device_node(dev, &dev_cursor);
    if (r <= 0)
        goto cleanup;

    r = find_device_location(&dev_cursor, ports);
    if (r <= 0)
        goto cleanup;
    depth = (unsigned int)r;

    r = read_device_properties(dev, &dev_cursor, ports[depth - 1]);
    if (r <= 0)
        goto cleanup;

    r = build_location_string(ports, depth, &dev->location);
    if (r < 0)
        goto cleanup;

    *rdev = dev;
    dev = NULL;
    r = 1;

cleanup:
    hs_device_unref(dev);
    return r;
}

static void free_controllers(void)
{
    for (unsigned int i = 0; i < controllers_count; i++)
        free(controllers[i]);
    DeleteCriticalSection(&controllers_lock);
}

static int populate_controllers(void)
{
    HDEVINFO set = NULL;
    SP_DEVINFO_DATA info;
    int r;

    if (controllers_count)
        return 0;

    if (controllers_lock_setup != 2) {
        if (!InterlockedCompareExchange(&controllers_lock_setup, 1, 0)) {
            InitializeCriticalSection(&controllers_lock);
            atexit(free_controllers);
            controllers_lock_setup = 2;
        } else {
            while (controllers_lock_setup != 2)
                continue;
        }
    }

    EnterCriticalSection(&controllers_lock);

    if (controllers_count) {
        r = 0;
        goto cleanup;
    }

    hs_log(HS_LOG_DEBUG, "Listing USB host controllers and root hubs");

    set = SetupDiGetClassDevs(&GUID_DEVINTERFACE_USB_HOST_CONTROLLER, NULL, NULL,
                              DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (!set) {
        r = hs_error(HS_ERROR_SYSTEM, "SetupDiGetClassDevs() failed: %s", hs_win32_strerror(0));
        goto cleanup;
    }

    info.cbSize = sizeof(info);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &info); i++) {
        struct device_cursor cursor;

        if (controllers_count == _HS_COUNTOF(controllers)) {
            hs_log(HS_LOG_WARNING, "Reached maximum controller ID %d, ignoring", UINT8_MAX);
            break;
        }

        if (!make_device_cursor(info.DevInst, &cursor))
            continue;
        if (!make_relative_cursor(&cursor, DEVINST_RELATIVE_CHILD, &cursor))
            continue;
        if (!is_root_usb_controller(cursor.id)) {
            hs_log(HS_LOG_WARNING, "Expected root hub device at '%s'", cursor.id);
            continue;
        }

        controllers[controllers_count] = strdup(cursor.id);
        if (!controllers[controllers_count]) {
            r = hs_error(HS_ERROR_MEMORY, NULL);
            goto cleanup;
        }
        hs_log(HS_LOG_DEBUG, "Found root USB hub '%s' with ID %u", cursor.id, controllers_count);
        controllers_count++;
    }

    r = 0;
cleanup:
    LeaveCriticalSection(&controllers_lock);
    if (set)
        SetupDiDestroyDeviceInfoList(set);
    return r;
}

static int enumerate_setup_class(const GUID *guid, const _hs_match_helper *match_helper,
                                 hs_enumerate_func *f, void *udata)
{
    HDEVINFO set = NULL;
    SP_DEVINFO_DATA info;
    hs_device *dev = NULL;
    int r;

    set = SetupDiGetClassDevs(guid, NULL, NULL, DIGCF_PRESENT);
    if (!set) {
        r = hs_error(HS_ERROR_SYSTEM, "SetupDiGetClassDevs() failed: %s", hs_win32_strerror(0));
        goto cleanup;
    }

    info.cbSize = sizeof(info);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &info); i++) {
        r = process_win32_device(info.DevInst, &dev);
        if (r < 0)
            goto cleanup;
        if (!r)
            continue;

        if (_hs_match_helper_match(match_helper, dev, &dev->match_udata)) {
            r = (*f)(dev, udata);
            hs_device_unref(dev);
            if (r)
                goto cleanup;
        } else {
            hs_device_unref(dev);
        }
    }

    r = 0;
cleanup:
    if (set)
        SetupDiDestroyDeviceInfoList(set);
    return r;
}

int enumerate(_hs_match_helper *match_helper, hs_enumerate_func *f, void *udata)
{
    int r;

    r = populate_controllers();
    if (r < 0)
        return r;

    for (unsigned int i = 0; i < _HS_COUNTOF(setup_classes); i++) {
        if (_hs_match_helper_has_type(match_helper, setup_classes[i].type)) {
            GUID guids[8];
            DWORD guids_count;
            BOOL success;

            success = SetupDiClassGuidsFromNameA(setup_classes[i].name, guids, _HS_COUNTOF(guids),
                                                 &guids_count);
            if (!success)
                return hs_error(HS_ERROR_SYSTEM, "SetupDiClassGuidsFromName('%s') failed: %s",
                                setup_classes[i].name, hs_win32_strerror(0));

            for (unsigned int j = 0; j < guids_count; j++) {
                r = enumerate_setup_class(&guids[j], match_helper, f, udata);
                if (r)
                    return r;
            }
        }
    }

    return 0;
}

struct enumerate_enumerate_context {
    hs_enumerate_func *f;
    void *udata;
};

static int enumerate_enumerate_callback(hs_device *dev, void *udata)
{
    struct enumerate_enumerate_context *ctx = (struct enumerate_enumerate_context *)udata;

    _hs_device_log(dev, "Enumerate");
    return (*ctx->f)(dev, ctx->udata);
}

int hs_enumerate(const hs_match_spec *matches, unsigned int count, hs_enumerate_func *f, void *udata)
{
    assert(f);

    _hs_match_helper match_helper = {0};
    struct enumerate_enumerate_context ctx;
    int r;

    r = _hs_match_helper_init(&match_helper, matches, count);
    if (r < 0)
        return r;

    ctx.f = f;
    ctx.udata = udata;

    r = enumerate(&match_helper, enumerate_enumerate_callback, &ctx);

    _hs_match_helper_release(&match_helper);
    return r;
}

static int post_event(hs_monitor *monitor, enum event_type event_type, const char *id)
{
    size_t id_len;
    struct event *event;
    int r;

    /* Extract the device instance ID part.
       - in: \\?\USB#Vid_2341&Pid_0042#85336303532351101252#{a5dcbf10-6530-11d2-901f-00c04fb951ed}
       - out: USB#Vid_2341&Pid_0042#85336303532351101252
       You may notice that paths from RegisterDeviceNotification() seem to start with '\\?\',
       which according to MSDN is the file namespace, not the device namespace '\\.\'. Oh well. */
    if (strncmp(id, "\\\\?\\", 4) == 0
            || strncmp(id, "\\\\.\\", 4) == 0
            || strncmp(id, "##.#", 4) == 0
            || strncmp(id, "##?#", 4) == 0)
        id += 4;
    id_len = strlen(id);
    if (id_len >= 39 && id[id_len - 39] == '#' && id[id_len - 38] == '{' && id[id_len - 1] == '}')
        id_len -= 39;

    if (id_len >= sizeof(event->device_key)) {
        hs_log(HS_LOG_WARNING, "Device instance ID string '%s' is too long, ignoring", id);
        return 0;
    }
    r = _hs_array_grow(&monitor->thread_events, 1);
    if (r < 0)
        return r;
    event = &monitor->thread_events.values[monitor->thread_events.count];
    event->type = event_type;
    memcpy(event->device_key, id, id_len);
    event->device_key[id_len] = 0;
    monitor->thread_events.count++;

    /* Normalize device instance ID, uppercase and replace '#' with '\'. Could not do it on msg,
       Windows may not like it. Maybe, not sure so don't try. */
    for (char *ptr = event->device_key; *ptr; ptr++) {
        if (*ptr == '#') {
            *ptr = '\\';
        } else if (*ptr >= 97 && *ptr <= 122) {
            *ptr = (char)(*ptr - 32);
        }
    }

    /* On Windows 7 (and maybe Windows 8), we don't get notifications for individual
       interfaces in composite devices. We need to search for them. */
    if (event_type == DEVICE_EVENT_ADDED && hs_win32_version() < HS_WIN32_VERSION_10) {
        DEVINST inst;
        CONFIGRET cret;

        cret = CM_Locate_DevNodeA(&inst, (DEVINSTID)event->device_key, CM_LOCATE_DEVNODE_NORMAL);
        if (cret != CR_SUCCESS) {
            hs_log(HS_LOG_DEBUG, "Device node '%s' does not exist: 0x%lx", event->device_key, cret);
            return 0;
        }

        struct device_cursor child_cursor;
        if (!make_device_cursor(inst, &child_cursor))
            return 0;
        if (!make_relative_cursor(&child_cursor, DEVINST_RELATIVE_CHILD, &child_cursor))
            return 0;

        do {
            r = post_event(monitor, DEVICE_EVENT_ADDED, child_cursor.id);
            if (r < 0)
                return r;
        } while (make_relative_cursor(&child_cursor, DEVINST_RELATIVE_SIBLING, &child_cursor));
    }

    return 0;
}

static LRESULT __stdcall window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    hs_monitor *monitor = (hs_monitor *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    int r;

    switch (msg) {
        case WM_DEVICECHANGE: {
            DEV_BROADCAST_DEVICEINTERFACE_A *msg2 = (DEV_BROADCAST_DEVICEINTERFACE_A *)lparam;

            if (msg2->dbcc_devicetype != DBT_DEVTYP_DEVICEINTERFACE)
                break;

            r = 0;
            switch (wparam) {
                case DBT_DEVICEARRIVAL: {
                    r = post_event(monitor, DEVICE_EVENT_ADDED, msg2->dbcc_name);
                } break;

                case DBT_DEVICEREMOVECOMPLETE: {
                    r = post_event(monitor, DEVICE_EVENT_REMOVED, msg2->dbcc_name);
                } break;
            }

            if (!r) {
                UINT_PTR timer;

                timer = SetTimer(hwnd, 1, 100, NULL);
                if (!timer)
                    r = hs_error(HS_ERROR_SYSTEM, "SetTimer() failed: %s", hs_win32_strerror(0));
            }

            if (r < 0) {
                EnterCriticalSection(&monitor->events_lock);
                monitor->thread_ret = r;
                SetEvent(monitor->thread_event);
                LeaveCriticalSection(&monitor->events_lock);
            }
        } break;

        case WM_TIMER: {
            if (CMP_WaitNoPendingInstallEvents(0) == WAIT_OBJECT_0) {
                KillTimer(hwnd, 1);

                EnterCriticalSection(&monitor->events_lock);
                r = _hs_array_grow(&monitor->events, monitor->thread_events.count);
                if (r < 0) {
                    monitor->thread_ret = r;
                } else {
                    memcpy(monitor->events.values + monitor->events.count,
                           monitor->thread_events.values,
                           monitor->thread_events.count * sizeof(*monitor->thread_events.values));
                    monitor->events.count += monitor->thread_events.count;
                    _hs_array_release(&monitor->thread_events);
                }
                SetEvent(monitor->thread_event);
                LeaveCriticalSection(&monitor->events_lock);
            }
        } break;

        case WM_CLOSE: {
            PostQuitMessage(0);
        } break;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

static void unregister_monitor_class(void)
{
    UnregisterClassA(MONITOR_CLASS_NAME, GetModuleHandle(NULL));
}

static unsigned int __stdcall monitor_thread(void *udata)
{
    _HS_UNUSED(udata);

    hs_monitor *monitor = (hs_monitor *)udata;

    WNDCLASSEXA cls = {0};
    ATOM cls_atom;
    DEV_BROADCAST_DEVICEINTERFACE_A filter = {0};
    HDEVNOTIFY notify_handle = NULL;
    MSG msg;
    int r;

    cls.cbSize = sizeof(cls);
    cls.hInstance = GetModuleHandle(NULL);
    cls.lpszClassName = MONITOR_CLASS_NAME;
    cls.lpfnWndProc = window_proc;

    /* If this fails, CreateWindow() will fail too so we can ignore errors here. This
       also takes care of any failure that may result from the class already existing. */
    cls_atom = RegisterClassExA(&cls);
    if (cls_atom)
        atexit(unregister_monitor_class);

    monitor->thread_hwnd = CreateWindowA(MONITOR_CLASS_NAME, MONITOR_CLASS_NAME, 0, 0, 0, 0, 0,
                                         HWND_MESSAGE, NULL, NULL, NULL);
    if (!monitor->thread_hwnd) {
        r = hs_error(HS_ERROR_SYSTEM, "CreateWindow() failed: %s", hs_win32_strerror(0));
        goto cleanup;
    }

    SetLastError(0);
    SetWindowLongPtr(monitor->thread_hwnd, GWLP_USERDATA, (LONG_PTR)monitor);
    if (GetLastError()) {
        r = hs_error(HS_ERROR_SYSTEM, "SetWindowLongPtr() failed: %s", hs_win32_strerror(0));
        goto cleanup;
    }

    filter.dbcc_size = sizeof(filter);
    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;

    /* We monitor everything because I cannot find an interface class to detect
       serial devices within an IAD, and RegisterDeviceNotification() does not
       support device setup class filtering. */
    notify_handle = RegisterDeviceNotificationA(monitor->thread_hwnd, &filter,
                                                DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES);
    if (!notify_handle) {
        r = hs_error(HS_ERROR_SYSTEM, "RegisterDeviceNotification() failed: %s", hs_win32_strerror(0));
        goto cleanup;
    }

    /* Our fake window is created and ready to receive device notifications,
       hs_monitor_new() can go on. */
    SetEvent(monitor->thread_event);

    /* As it turns out, GetMessage() cannot fail if the parameters are correct.
       https://blogs.msdn.microsoft.com/oldnewthing/20130322-00/?p=4873/ */
    while (GetMessage(&msg, NULL, 0, 0) != 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    r = 0;
cleanup:
    if (notify_handle)
        UnregisterDeviceNotification(notify_handle);
    if (monitor->thread_hwnd)
        DestroyWindow(monitor->thread_hwnd);
    if (r < 0) {
        monitor->thread_ret = r;
        SetEvent(monitor->thread_event);
    }
    return 0;
}

static int monitor_enumerate_callback(hs_device *dev, void *udata)
{
    hs_monitor *monitor = (hs_monitor *)udata;
    return _hs_monitor_add(&monitor->devices, dev, NULL, NULL);
}

/* Monitoring device changes on Windows involves a window to receive device notifications on the
   thread message queue. Unfortunately we can't poll on message queues so instead, we make a
   background thread to get device notifications, and tell us about it using Win32 events which
   we can poll. */
int hs_monitor_new(const hs_match_spec *matches, unsigned int count, hs_monitor **rmonitor)
{
    assert(rmonitor);

    hs_monitor *monitor;
    int r;

    monitor = (hs_monitor *)calloc(1, sizeof(*monitor));
    if (!monitor) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto error;
    }

    r = _hs_match_helper_init(&monitor->match_helper, matches, count);
    if (r < 0)
        goto error;

    r = _hs_htable_init(&monitor->devices, 64);
    if (r < 0)
        goto error;

    InitializeCriticalSection(&monitor->events_lock);
    monitor->thread_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!monitor->thread_event) {
        r = hs_error(HS_ERROR_SYSTEM, "CreateEvent() failed: %s", hs_win32_strerror(0));
        goto error;
    }

    *rmonitor = monitor;
    return 0;

error:
    hs_monitor_free(monitor);
    return r;
}

void hs_monitor_free(hs_monitor *monitor)
{
    if (monitor) {
        hs_monitor_stop(monitor);

        DeleteCriticalSection(&monitor->events_lock);
        if (monitor->thread_event)
            CloseHandle(monitor->thread_event);

        _hs_monitor_clear_devices(&monitor->devices);
        _hs_htable_release(&monitor->devices);
        _hs_match_helper_release(&monitor->match_helper);
    }

    free(monitor);
}

hs_handle hs_monitor_get_poll_handle(const hs_monitor *monitor)
{
    assert(monitor);
    return monitor->thread_event;
}

int hs_monitor_start(hs_monitor *monitor)
{
    assert(monitor);
    assert(!monitor->thread);

    int r;

    if (monitor->thread)
        return 0;

    /* We can't create our fake window here, because the messages would be posted to this thread's
       message queue and not to the monitoring thread. So instead, the background thread creates
       its own window and we wait for it to signal us before we continue. */
    monitor->thread = (HANDLE)_beginthreadex(NULL, 0, monitor_thread, monitor, 0, NULL);
    if (!monitor->thread) {
        r = hs_error(HS_ERROR_SYSTEM, "_beginthreadex() failed: %s", hs_win32_strerror(0));
        goto error;
    }

    WaitForSingleObject(monitor->thread_event, INFINITE);
    if (monitor->thread_ret < 0) {
        r = monitor->thread_ret;
        goto error;
    }
    ResetEvent(monitor->thread_event);

    r = enumerate(&monitor->match_helper, monitor_enumerate_callback, monitor);
    if (r < 0)
        goto error;

    return 0;

error:
    hs_monitor_stop(monitor);
    return r;
}

void hs_monitor_stop(hs_monitor *monitor)
{
    assert(monitor);

    if (!monitor->thread)
        return;

    _hs_monitor_clear_devices(&monitor->devices);

    if (monitor->thread_hwnd) {
        PostMessage(monitor->thread_hwnd, WM_CLOSE, 0, 0);
        WaitForSingleObject(monitor->thread, INFINITE);
    }
    CloseHandle(monitor->thread);
    monitor->thread = NULL;

    _hs_array_release(&monitor->events);
    _hs_array_release(&monitor->thread_events);
    _hs_array_release(&monitor->refresh_events);
}

static int process_arrival_event(hs_monitor *monitor, const char *key, hs_enumerate_func *f,
                                 void *udata)
{
    DEVINST inst;
    hs_device *dev = NULL;
    CONFIGRET cret;
    int r;

    cret = CM_Locate_DevNodeA(&inst, (DEVINSTID)key, CM_LOCATE_DEVNODE_NORMAL);
    if (cret != CR_SUCCESS) {
        hs_log(HS_LOG_DEBUG, "Device node '%s' does not exist: 0x%lx", key, cret);
        return 0;
    }

    r = process_win32_device(inst, &dev);
    if (r <= 0)
        return r;

    r = _hs_match_helper_match(&monitor->match_helper, dev, &dev->match_udata);
    if (r)
        r = _hs_monitor_add(&monitor->devices, dev, f, udata);
    hs_device_unref(dev);

    return r;
}

int hs_monitor_refresh(hs_monitor *monitor, hs_enumerate_func *f, void *udata)
{
    assert(monitor);

    unsigned int event_idx = 0;
    int r;

    if (!monitor->thread)
        return 0;

    if (!monitor->refresh_events.count) {
        /* We don't want to keep the lock for too long, so move all device events to our
           own array and let the background thread work and process Win32 events. */
        EnterCriticalSection(&monitor->events_lock);
        monitor->refresh_events = monitor->events;
        memset(&monitor->events, 0, sizeof(monitor->events));
        r = monitor->thread_ret;
        monitor->thread_ret = 0;
        LeaveCriticalSection(&monitor->events_lock);

        if (r < 0)
            goto cleanup;
    }

    for (; event_idx < monitor->refresh_events.count; event_idx++) {
        struct event *event = &monitor->refresh_events.values[event_idx];

        switch (event->type) {
            case DEVICE_EVENT_ADDED: {
                hs_log(HS_LOG_DEBUG, "Received arrival notification for device '%s'",
                       event->device_key);
                r = process_arrival_event(monitor, event->device_key, f, udata);
                if (r)
                    goto cleanup;
            } break;

            case DEVICE_EVENT_REMOVED: {
                hs_log(HS_LOG_DEBUG, "Received removal notification for device '%s'",
                       event->device_key);
                _hs_monitor_remove(&monitor->devices, event->device_key, f, udata);
            } break;
        }
    }

    r = 0;
cleanup:
    /* If an error occurs, there may be unprocessed notifications. Keep them in
       monitor->refresh_events for the next time this function is called. */
    _hs_array_remove(&monitor->refresh_events, 0, event_idx);
    EnterCriticalSection(&monitor->events_lock);
    if (!monitor->refresh_events.count && !monitor->events.count)
        ResetEvent(monitor->thread_event);
    LeaveCriticalSection(&monitor->events_lock);
    return r;
}

int hs_monitor_list(hs_monitor *monitor, hs_enumerate_func *f, void *udata)
{
    return _hs_monitor_list(&monitor->devices, f, udata);
}

// platform_win32.c
// ------------------------------------

// #include "common_priv.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// #include "platform.h"

typedef LONG NTAPI RtlGetVersion_func(OSVERSIONINFOW *info);

uint64_t hs_millis(void)
{
    return GetTickCount64();
}

void hs_delay(unsigned int ms)
{
    Sleep(ms);
}

int hs_poll(hs_poll_source *sources, unsigned int count, int timeout)
{
    assert(sources);
    assert(count);
    assert(count <= HS_POLL_MAX_SOURCES);

    HANDLE handles[HS_POLL_MAX_SOURCES];

    for (unsigned int i = 0; i < count; i++) {
        handles[i] = sources[i].desc;
        sources[i].ready = 0;
    }

    DWORD ret = WaitForMultipleObjects((DWORD)count, handles, FALSE,
                                       timeout < 0 ? INFINITE : (DWORD)timeout);
    if (ret == WAIT_FAILED)
        return hs_error(HS_ERROR_SYSTEM, "WaitForMultipleObjects() failed: %s",
                        hs_win32_strerror(0));

    for (unsigned int i = 0; i < count; i++)
        sources[i].ready = (i == ret);

    return ret < count;
}

const char *hs_win32_strerror(DWORD err)
{
    static _HS_THREAD_LOCAL char buf[256];
    char *ptr;
    DWORD r;

    if (!err)
        err = GetLastError();

    r = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                       err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, sizeof(buf), NULL);

    if (r) {
        ptr = buf + strlen(buf);
        // FormatMessage adds newlines, remove them
        while (ptr > buf && (ptr[-1] == '\n' || ptr[-1] == '\r'))
            ptr--;
        *ptr = 0;
    } else {
        sprintf(buf, "Unknown error 0x%08lx", err);
    }

    return buf;
}

uint32_t hs_win32_version(void)
{
    static uint32_t version;

    if (!version) {
        OSVERSIONINFOW info;

        // Windows 8.1 broke GetVersionEx, so bypass the intermediary
        info.dwOSVersionInfoSize = sizeof(info);

        RtlGetVersion_func *RtlGetVersion =
            (RtlGetVersion_func *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion");
        RtlGetVersion(&info);

        version = info.dwMajorVersion * 100 + info.dwMinorVersion;
    }

    return version;
}

// serial_win32.c
// ------------------------------------

// #include "common_priv.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// #include "device_priv.h"
// #include "platform.h"
// #include "serial.h"

int hs_serial_set_config(hs_port *port, const hs_serial_config *config)
{
    assert(port);
    assert(config);

    DCB dcb;
    BOOL success;

    dcb.DCBlength = sizeof(dcb);
    success = GetCommState(port->u.handle.h, &dcb);
    if (!success)
        return hs_error(HS_ERROR_SYSTEM, "GetCommState() failed on '%s': %s", port->dev->path,
                        hs_win32_strerror(0));

    switch (config->baudrate) {
        case 0: {} break;

        case 110:
        case 134:
        case 150:
        case 200:
        case 300:
        case 600:
        case 1200:
        case 1800:
        case 2400:
        case 4800:
        case 9600:
        case 19200:
        case 38400:
        case 57600:
        case 115200:
        case 230400: {
            dcb.BaudRate = config->baudrate;
        } break;

        default: {
            return hs_error(HS_ERROR_SYSTEM, "Unsupported baud rate value: %u", config->baudrate);
        } break;
    }

    switch (config->databits) {
        case 0: {} break;

        case 5:
        case 6:
        case 7:
        case 8: {
            dcb.ByteSize = (BYTE)config->databits;
        } break;

        default: {
            return hs_error(HS_ERROR_SYSTEM, "Invalid data bits setting: %u", config->databits);
        } break;
    }

    switch (config->stopbits) {
        case 0: {} break;

        case 1: { dcb.StopBits = ONESTOPBIT; } break;
        case 2: { dcb.StopBits = TWOSTOPBITS; } break;

        default: {
            return hs_error(HS_ERROR_SYSTEM, "Invalid stop bits setting: %u", config->stopbits);
        } break;
    }

    switch (config->parity) {
        case 0: {} break;

        case HS_SERIAL_CONFIG_PARITY_OFF: {
            dcb.fParity = FALSE;
            dcb.Parity = NOPARITY;
        } break;
        case HS_SERIAL_CONFIG_PARITY_EVEN: {
            dcb.fParity = TRUE;
            dcb.Parity = EVENPARITY;
        } break;
        case HS_SERIAL_CONFIG_PARITY_ODD: {
            dcb.fParity = TRUE;
            dcb.Parity = ODDPARITY;
        } break;
        case HS_SERIAL_CONFIG_PARITY_MARK: {
            dcb.fParity = TRUE;
            dcb.Parity = MARKPARITY;
        } break;
        case HS_SERIAL_CONFIG_PARITY_SPACE: {
            dcb.fParity = TRUE;
            dcb.Parity = SPACEPARITY;
        } break;

        default: {
            return hs_error(HS_ERROR_SYSTEM, "Invalid parity setting: %d", config->parity);
        } break;
    }

    switch (config->rts) {
        case 0: {} break;

        case HS_SERIAL_CONFIG_RTS_OFF: {
            dcb.fRtsControl = RTS_CONTROL_DISABLE;
            dcb.fOutxCtsFlow = FALSE;
        } break;
        case HS_SERIAL_CONFIG_RTS_ON: {
            dcb.fRtsControl = RTS_CONTROL_ENABLE;
            dcb.fOutxCtsFlow = FALSE;
        } break;
        case HS_SERIAL_CONFIG_RTS_FLOW: {
            dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
            dcb.fOutxCtsFlow = TRUE;
        } break;

        default: {
            return hs_error(HS_ERROR_SYSTEM, "Invalid RTS setting: %d", config->rts);
        } break;
    }

    switch (config->dtr) {
        case 0: {} break;

        case HS_SERIAL_CONFIG_DTR_OFF: {
            dcb.fDtrControl = DTR_CONTROL_DISABLE;
            dcb.fOutxDsrFlow = FALSE;
        } break;
        case HS_SERIAL_CONFIG_DTR_ON: {
            dcb.fDtrControl = DTR_CONTROL_ENABLE;
            dcb.fOutxDsrFlow = FALSE;
        } break;

        default: {
            return hs_error(HS_ERROR_SYSTEM, "Invalid DTR setting: %d", config->dtr);
        } break;
    }

    switch (config->xonxoff) {
        case 0: {} break;

        case HS_SERIAL_CONFIG_XONXOFF_OFF: {
            dcb.fOutX = FALSE;
            dcb.fInX = FALSE;
        } break;
        case HS_SERIAL_CONFIG_XONXOFF_IN: {
            dcb.fOutX = FALSE;
            dcb.fInX = TRUE;
        } break;
        case HS_SERIAL_CONFIG_XONXOFF_OUT: {
            dcb.fOutX = TRUE;
            dcb.fInX = FALSE;
        } break;
        case HS_SERIAL_CONFIG_XONXOFF_INOUT: {
            dcb.fOutX = TRUE;
            dcb.fInX = TRUE;
        } break;

        default: {
            return hs_error(HS_ERROR_SYSTEM, "Invalid XON/XOFF setting: %d", config->xonxoff);
        } break;
    }

    success = SetCommState(port->u.handle.h, &dcb);
    if (!success)
        return hs_error(HS_ERROR_SYSTEM, "SetCommState() failed on '%s': %s",
                        port->dev->path, hs_win32_strerror(0));

    return 0;
}

int hs_serial_get_config(hs_port *port, hs_serial_config *config)
{
    assert(port);
    assert(config);

    DCB dcb;
    BOOL success;

    dcb.DCBlength = sizeof(dcb);
    success = GetCommState(port->u.handle.h, &dcb);
    if (!success)
        return hs_error(HS_ERROR_SYSTEM, "GetCommState() failed on '%s': %s", port->dev->path,
                        hs_win32_strerror(0));

    /* 0 is the INVALID value for all parameters, we keep that value if we can't interpret
       a DCB setting (only a cross-platform subset of it is exposed in hs_serial_config). */
    memset(config, 0, sizeof(*config));

    config->baudrate = dcb.BaudRate;
    config->databits = dcb.ByteSize;

    // There is also ONE5STOPBITS, ignore it for now (and ever, probably)
    switch (dcb.StopBits) {
        case ONESTOPBIT: { config->stopbits = 1; } break;
        case TWOSTOPBITS: { config->stopbits = 2; } break;
    }

    if (dcb.fParity) {
        switch (dcb.Parity) {
            case NOPARITY: { config->parity = HS_SERIAL_CONFIG_PARITY_OFF; } break;
            case EVENPARITY: { config->parity = HS_SERIAL_CONFIG_PARITY_EVEN; } break;
            case ODDPARITY: { config->parity = HS_SERIAL_CONFIG_PARITY_ODD; } break;
            case MARKPARITY: { config->parity = HS_SERIAL_CONFIG_PARITY_MARK; } break;
            case SPACEPARITY: { config->parity = HS_SERIAL_CONFIG_PARITY_SPACE; } break;
        }
    } else {
        config->parity = HS_SERIAL_CONFIG_PARITY_OFF;
    }

    switch (dcb.fRtsControl) {
        case RTS_CONTROL_DISABLE: { config->rts = HS_SERIAL_CONFIG_RTS_OFF; } break;
        case RTS_CONTROL_ENABLE: { config->rts = HS_SERIAL_CONFIG_RTS_ON; } break;
        case RTS_CONTROL_HANDSHAKE: { config->rts = HS_SERIAL_CONFIG_RTS_FLOW; } break;
    }

    switch (dcb.fDtrControl) {
        case DTR_CONTROL_DISABLE: { config->dtr = HS_SERIAL_CONFIG_DTR_OFF; } break;
        case DTR_CONTROL_ENABLE: { config->dtr = HS_SERIAL_CONFIG_DTR_ON; } break;
    }

    if (dcb.fInX && dcb.fOutX) {
        config->xonxoff = HS_SERIAL_CONFIG_XONXOFF_INOUT;
    } else if (dcb.fInX) {
        config->xonxoff = HS_SERIAL_CONFIG_XONXOFF_IN;
    } else if (dcb.fOutX) {
        config->xonxoff = HS_SERIAL_CONFIG_XONXOFF_OUT;
    } else {
        config->xonxoff = HS_SERIAL_CONFIG_XONXOFF_OFF;
    }

    return 0;
}

ssize_t hs_serial_read(hs_port *port, uint8_t *buf, size_t size, int timeout)
{
    assert(port);
    assert(port->dev->type == HS_DEVICE_TYPE_SERIAL);
    assert(port->mode & HS_PORT_MODE_READ);
    assert(buf);
    assert(size);

    if (port->u.handle.read_status < 0) {
        // Could be a transient error, try to restart it
        _hs_win32_start_async_read(port);
        if (port->u.handle.read_status < 0)
            return port->u.handle.read_status;
    }

    /* Serial devices are stream-based. If we don't have any data yet, see if our asynchronous
       read request has returned anything. Then we can just give the user the data we have, until
       our buffer is empty. We can't just discard stuff, unlike what we do for long HID messages. */
    if (!port->u.handle.read_len) {
        _hs_win32_finalize_async_read(port, timeout);
        if (port->u.handle.read_status <= 0)
            return port->u.handle.read_status;
    }

    if (size > port->u.handle.read_len)
        size = port->u.handle.read_len;
    memcpy(buf, port->u.handle.read_ptr, size);
    port->u.handle.read_ptr += size;
    port->u.handle.read_len -= size;

    /* Our buffer has been fully read, start a new asynchonous request. I don't know how
       much latency this brings. Maybe double buffering would help, but not before any concrete
       benchmarking is done. */
    if (!port->u.handle.read_len) {
        hs_error_mask(HS_ERROR_IO);
        _hs_win32_start_async_read(port);
        hs_error_unmask();
    }

    return (ssize_t)size;
}

ssize_t hs_serial_write(hs_port *port, const uint8_t *buf, size_t size, int timeout)
{
    assert(port);
    assert(port->dev->type == HS_DEVICE_TYPE_SERIAL);
    assert(port->mode & HS_PORT_MODE_WRITE);
    assert(buf);
    
    if (!size)
        return 0;

    return _hs_win32_write_sync(port, buf, size, timeout);
}

    #elif defined(__APPLE__)
// device_posix.c
// ------------------------------------

// #include "common_priv.h"
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
// #include "device_priv.h"
// #include "platform.h"

int _hs_open_file_port(hs_device *dev, hs_port_mode mode, hs_port **rport)
{
    hs_port *port;
#ifdef __APPLE__
    unsigned int retry = 4;
#endif
    int fd_flags;
    int r;

    port = (hs_port *)calloc(1, sizeof(*port));
    if (!port) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto error;
    }
    port->type = dev->type;
    port->u.file.fd = -1;

    port->mode = mode;
    port->path = dev->path;
    port->dev = hs_device_ref(dev);

    fd_flags = O_CLOEXEC | O_NOCTTY | O_NONBLOCK;
    switch (mode) {
        case HS_PORT_MODE_READ: { fd_flags |= O_RDONLY; } break;
        case HS_PORT_MODE_WRITE: { fd_flags |= O_WRONLY; } break;
        case HS_PORT_MODE_RW: { fd_flags |= O_RDWR; } break;
    }

restart:
    port->u.file.fd = open(dev->path, fd_flags);
    if (port->u.file.fd < 0) {
        switch (errno) {
            case EINTR: {
                goto restart;
            } break;

            case EACCES: {
                r = hs_error(HS_ERROR_ACCESS, "Permission denied for device '%s'", dev->path);
            } break;
            case EIO:
            case ENXIO:
            case ENODEV: {
                r = hs_error(HS_ERROR_IO, "I/O error while opening device '%s'", dev->path);
            } break;
            case ENOENT:
            case ENOTDIR: {
                r = hs_error(HS_ERROR_NOT_FOUND, "Device '%s' not found", dev->path);
            } break;

#ifdef __APPLE__
            /* On El Capitan (and maybe before), the open fails for some time (around 40 - 50 ms
               on my computer) after the device notification. */
            case EBUSY: {
                if (retry--) {
                    usleep(20000);
                    goto restart;
                }
            } // fallthrough
#endif

            default: {
                r = hs_error(HS_ERROR_SYSTEM, "open('%s') failed: %s", dev->path, strerror(errno));
            } break;
        }
        goto error;
    }

    if (dev->type == HS_DEVICE_TYPE_SERIAL) {
        struct termios tio;
        int modem_bits;

        r = tcgetattr(port->u.file.fd, &tio);
        if (r < 0) {
            r = hs_error(HS_ERROR_SYSTEM, "tcgetattr() failed on '%s': %s", dev->path,
                         strerror(errno));
            goto error;
        }

        /* Use raw I/O and sane settings, set DTR by default even on platforms that don't
           enforce that. */
        cfmakeraw(&tio);
        tio.c_cc[VMIN] = 0;
        tio.c_cc[VTIME] = 0;
        tio.c_cflag |= CLOCAL | CREAD | HUPCL;
        modem_bits = TIOCM_DTR;

        r = tcsetattr(port->u.file.fd, TCSANOW, &tio);
        if (r < 0) {
            r = hs_error(HS_ERROR_SYSTEM, "tcsetattr() failed on '%s': %s", dev->path,
                         strerror(errno));
            goto error;
        }
        r = ioctl(port->u.file.fd, TIOCMBIS, &modem_bits);
        if (r < 0) {
            r = hs_error(HS_ERROR_SYSTEM, "ioctl(TIOCMBIS, TIOCM_DTR) failed on '%s': %s",
                         dev->path, strerror(errno));
            goto error;
        }
        r = tcflush(port->u.file.fd, TCIFLUSH);
        if (r < 0) {
            r = hs_error(HS_ERROR_SYSTEM, "tcflush(TCIFLUSH) failed on '%s': %s",
                         dev->path, strerror(errno));
            goto error;
        }
    }
#ifdef __linux__
    else if (dev->type == HS_DEVICE_TYPE_HID) {
        port->u.file.numbered_hid_reports = dev->u.hid.numbered_reports;
    }
#endif

    *rport = port;
    return 0;

error:
    hs_port_close(port);
    return r;
}

void _hs_close_file_port(hs_port *port)
{
    if (port) {
#ifdef __linux__
        // Only used for hidraw to work around a bug on old kernels
        free(port->u.file.read_buf);
#endif

        close(port->u.file.fd);
        hs_device_unref(port->dev);
    }

    free(port);
}

hs_handle _hs_get_file_port_poll_handle(const hs_port *port)
{
    return port->u.file.fd;
}

// hid_darwin.c
// ------------------------------------

// #include "common_priv.h"
#include <CoreFoundation/CFRunLoop.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/hid/IOHIDDevice.h>
#include <poll.h>
#include <pthread.h>
#include <unistd.h>
// #include "array.h"
// #include "device_priv.h"
// #include "hid.h"
// #include "platform.h"

struct hid_report {
    size_t size;
    uint8_t *data;
};

struct _hs_hid_darwin {
    io_service_t service;
    union {
        IOHIDDeviceRef hid_ref;
    };

    uint8_t *read_buf;
    size_t read_size;

    pthread_mutex_t mutex;
    bool mutex_init;
    int poll_pipe[2];
    int thread_ret;

    _HS_ARRAY(struct hid_report) reports;

    pthread_t read_thread;
    pthread_cond_t cond;
    bool cond_init;

    CFRunLoopRef thread_loop;
    CFRunLoopSourceRef shutdown_source;
    bool device_removed;
};

#define MAX_REPORT_QUEUE_SIZE 128

static void fire_hid_poll_handle(struct _hs_hid_darwin *hid)
{
    char buf = '.';
    write(hid->poll_pipe[1], &buf, 1);
}

static void reset_hid_poll_handle(struct _hs_hid_darwin *hid)
{
    char buf;
    read(hid->poll_pipe[0], &buf, 1);
}

static void hid_removal_callback(void *ctx, IOReturn result, void *sender)
{
    _HS_UNUSED(result);
    _HS_UNUSED(sender);

    hs_port *port = (hs_port *)ctx;
    struct _hs_hid_darwin *hid = port->u.hid;

    pthread_mutex_lock(&hid->mutex);
    hid->device_removed = true;
    CFRunLoopSourceSignal(hid->shutdown_source);
    pthread_mutex_unlock(&hid->mutex);

    fire_hid_poll_handle(hid);
}

static void hid_report_callback(void *ctx, IOReturn result, void *sender,
                                IOHIDReportType report_type, uint32_t report_id,
                                uint8_t *report_data, CFIndex report_size)
{
    _HS_UNUSED(result);
    _HS_UNUSED(sender);

    if (report_type != kIOHIDReportTypeInput)
        return;

    hs_port *port = (hs_port *)ctx;
    struct _hs_hid_darwin *hid = port->u.hid;
    struct hid_report *report;
    bool was_empty;
    int r;

    pthread_mutex_lock(&hid->mutex);

    was_empty = !hid->reports.count;
    if (hid->reports.count == MAX_REPORT_QUEUE_SIZE) {
        r = 0;
        goto cleanup;
    }

    r = _hs_array_grow(&hid->reports, 1);
    if (r < 0)
        goto cleanup;
    report = hid->reports.values + hid->reports.count;
    if (!report->data) {
        // Don't forget the leading report ID
        report->data = (uint8_t *)malloc(hid->read_size + 1);
        if (!report->data) {
            r = hs_error(HS_ERROR_MEMORY, NULL);
            goto cleanup;
        }
    }

    /* You never know, even if hid->red_size is supposed to be the maximum
       input report size. */
    if (report_size > (CFIndex)hid->read_size)
        report_size = (CFIndex)hid->read_size;

    report->data[0] = (uint8_t)report_id;
    memcpy(report->data + 1, report_data, report_size);
    report->size = (size_t)report_size + 1;

    hid->reports.count++;

    r = 0;
cleanup:
    if (r < 0)
        hid->thread_ret = r;
    pthread_mutex_unlock(&hid->mutex);
    if (was_empty)
        fire_hid_poll_handle(hid);
}

static void *hid_read_thread(void *ptr)
{
    hs_port *port = (hs_port *)ptr;
    struct _hs_hid_darwin *hid = port->u.hid;
    CFRunLoopSourceContext shutdown_ctx = {0};
    int r;

    pthread_mutex_lock(&hid->mutex);

    hid->thread_loop = CFRunLoopGetCurrent();

    shutdown_ctx.info = hid->thread_loop;
    shutdown_ctx.perform = (void (*)(void *))CFRunLoopStop;
    /* close_hid_device() could be called before the loop is running, while this thread is between
       pthread_barrier_wait() and CFRunLoopRun(). That's the purpose of the shutdown source. */
    hid->shutdown_source = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &shutdown_ctx);
    if (!hid->shutdown_source) {
        r = hs_error(HS_ERROR_SYSTEM, "CFRunLoopSourceCreate() failed");
        goto error;
    }

    CFRunLoopAddSource(hid->thread_loop, hid->shutdown_source, kCFRunLoopCommonModes);
    IOHIDDeviceScheduleWithRunLoop(hid->hid_ref, hid->thread_loop, kCFRunLoopCommonModes);

    // This thread is ready, open_hid_device() can carry on
    hid->thread_ret = 1;
    pthread_cond_signal(&hid->cond);
    pthread_mutex_unlock(&hid->mutex);

    CFRunLoopRun();

    IOHIDDeviceUnscheduleFromRunLoop(hid->hid_ref, hid->thread_loop, kCFRunLoopCommonModes);

    pthread_mutex_lock(&hid->mutex);
    hid->thread_loop = NULL;
    pthread_mutex_unlock(&hid->mutex);

    return NULL;

error:
    hid->thread_ret = r;
    pthread_cond_signal(&hid->cond);
    pthread_mutex_unlock(&hid->mutex);
    return NULL;
}

static bool get_hid_device_property_number(IOHIDDeviceRef ref, CFStringRef prop,
                                           CFNumberType type, void *rn)
{
    CFTypeRef data = IOHIDDeviceGetProperty(ref, prop);
    if (!data || CFGetTypeID(data) != CFNumberGetTypeID())
        return false;

    return CFNumberGetValue((CFNumberRef)data, type, rn);
}

int _hs_darwin_open_hid_port(hs_device *dev, hs_port_mode mode, hs_port **rport)
{
    hs_port *port;
    struct _hs_hid_darwin *hid;
    kern_return_t kret;
    int r;

    port = (hs_port *)calloc(1, _HS_ALIGN_SIZE_FOR_TYPE(sizeof(*port), struct _hs_hid_darwin) +
                                sizeof(struct _hs_hid_darwin));
    if (!port) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto error;
    }
    port->type = dev->type;
    port->u.hid = (struct _hs_hid_darwin *)((char *)port +
                                            _HS_ALIGN_SIZE_FOR_TYPE(sizeof(*port), struct _hs_hid_darwin));
    hid = port->u.hid;
    hid->poll_pipe[0] = -1;
    hid->poll_pipe[1] = -1;

    port->mode = mode;
    port->path = dev->path;
    port->dev = hs_device_ref(dev);

    hid->service = IORegistryEntryFromPath(kIOMasterPortDefault, dev->path);
    if (!hid->service) {
        r = hs_error(HS_ERROR_NOT_FOUND, "Device '%s' not found", dev->path);
        goto error;
    }

    hid->hid_ref = IOHIDDeviceCreate(kCFAllocatorDefault, hid->service);
    if (!hid->hid_ref) {
        r = hs_error(HS_ERROR_NOT_FOUND, "Device '%s' not found", dev->path);
        goto error;
    }

    kret = IOHIDDeviceOpen(hid->hid_ref, 0);
    if (kret != kIOReturnSuccess) {
        r = hs_error(HS_ERROR_SYSTEM, "Failed to open HID device '%s'", dev->path);
        goto error;
    }

    IOHIDDeviceRegisterRemovalCallback(hid->hid_ref, hid_removal_callback, port);

    if (mode & HS_PORT_MODE_READ) {
        r = get_hid_device_property_number(hid->hid_ref, CFSTR(kIOHIDMaxInputReportSizeKey),
                                           kCFNumberSInt32Type, &hid->read_size);
        if (!r) {
            r = hs_error(HS_ERROR_SYSTEM, "HID device '%s' has no valid report size key", dev->path);
            goto error;
        }
        hid->read_buf = (uint8_t *)malloc(hid->read_size);
        if (!hid->read_buf) {
            r = hs_error(HS_ERROR_MEMORY, NULL);
            goto error;
        }

        IOHIDDeviceRegisterInputReportCallback(hid->hid_ref, hid->read_buf,
                                               (CFIndex)hid->read_size, hid_report_callback, port);

        r = pipe(hid->poll_pipe);
        if (r < 0) {
            r = hs_error(HS_ERROR_SYSTEM, "pipe() failed: %s", strerror(errno));
            goto error;
        }
        fcntl(hid->poll_pipe[0], F_SETFL, fcntl(hid->poll_pipe[0], F_GETFL, 0) | O_NONBLOCK);
        fcntl(hid->poll_pipe[1], F_SETFL, fcntl(hid->poll_pipe[1], F_GETFL, 0) | O_NONBLOCK);

        r = pthread_mutex_init(&hid->mutex, NULL);
        if (r < 0) {
            r = hs_error(HS_ERROR_SYSTEM, "pthread_mutex_init() failed: %s", strerror(r));
            goto error;
        }
        hid->mutex_init = true;

        r = pthread_cond_init(&hid->cond, NULL);
        if (r < 0) {
            r = hs_error(HS_ERROR_SYSTEM, "pthread_cond_init() failed: %s", strerror(r));
            goto error;
        }
        hid->cond_init = true;

        pthread_mutex_lock(&hid->mutex);

        r = pthread_create(&hid->read_thread, NULL, hid_read_thread, port);
        if (r) {
            r = hs_error(HS_ERROR_SYSTEM, "pthread_create() failed: %s", strerror(r));
            goto error;
        }

        /* Barriers are great for this, but OSX doesn't have those... And since it's the only
           place we would use them, it's probably not worth it to have a custom implementation. */
        while (!hid->thread_ret)
            pthread_cond_wait(&hid->cond, &hid->mutex);
        r = hid->thread_ret;
        hid->thread_ret = 0;
        pthread_mutex_unlock(&hid->mutex);
        if (r < 0)
            goto error;
    }

    *rport = port;
    return 0;

error:
    hs_port_close(port);
    return r;
}

void _hs_darwin_close_hid_port(hs_port *port)
{
    if (port) {
        struct _hs_hid_darwin *hid = port->u.hid;

        if (hid->shutdown_source) {
            pthread_mutex_lock(&hid->mutex);

            if (hid->thread_loop) {
                CFRunLoopSourceSignal(hid->shutdown_source);
                CFRunLoopWakeUp(hid->thread_loop);
            }

            pthread_mutex_unlock(&hid->mutex);
            pthread_join(hid->read_thread, NULL);

            CFRelease(hid->shutdown_source);
        }

        if (hid->cond_init)
            pthread_cond_destroy(&hid->cond);
        if (hid->mutex_init)
            pthread_mutex_destroy(&hid->mutex);

        for (size_t i = 0; i < hid->reports.count; i++) {
            struct hid_report *report = &hid->reports.values[i];
            free(report->data);
        }
        _hs_array_release(&hid->reports);

        close(hid->poll_pipe[0]);
        close(hid->poll_pipe[1]);

        free(hid->read_buf);

        if (hid->hid_ref) {
            IOHIDDeviceClose(hid->hid_ref, 0);
            CFRelease(hid->hid_ref);
        }
        if (hid->service)
            IOObjectRelease(hid->service);

        hs_device_unref(port->dev);
    }

    free(port);
}

hs_handle _hs_darwin_get_hid_port_poll_handle(const hs_port *port)
{
    return port->u.hid->poll_pipe[0];
}

ssize_t hs_hid_read(hs_port *port, uint8_t *buf, size_t size, int timeout)
{
    assert(port);
    assert(port->type == HS_DEVICE_TYPE_HID);
    assert(port->mode & HS_PORT_MODE_READ);
    assert(buf);
    assert(size);

    struct _hs_hid_darwin *hid = port->u.hid;
    struct hid_report *report;
    ssize_t r;

    if (hid->device_removed)
        return hs_error(HS_ERROR_IO, "Device '%s' was removed", port->path);

    if (timeout) {
        struct pollfd pfd;
        uint64_t start;

        pfd.events = POLLIN;
        pfd.fd = hid->poll_pipe[0];

        start = hs_millis();
restart:
        r = poll(&pfd, 1, hs_adjust_timeout(timeout, start));
        if (r < 0) {
            if (errno == EINTR)
                goto restart;

            return hs_error(HS_ERROR_SYSTEM, "poll('%s') failed: %s", port->path, strerror(errno));
        }
        if (!r)
            return 0;
    }

    pthread_mutex_lock(&hid->mutex);

    if (hid->thread_ret < 0) {
        r = hid->thread_ret;
        hid->thread_ret = 0;
        goto cleanup;
    }
    if (!hid->reports.count) {
        r = 0;
        goto cleanup;
    }

    report = &hid->reports.values[0];
    if (size > report->size)
        size = report->size;
    memcpy(buf, report->data, size);
    r = (ssize_t)size;

    // Circular buffer would be more appropriate. Later.
    _hs_array_remove(&hid->reports, 0, 1);

cleanup:
    if (!hid->reports.count)
        reset_hid_poll_handle(hid);
    pthread_mutex_unlock(&hid->mutex);
    return r;
}

static ssize_t send_report(hs_port *port, IOHIDReportType type, const uint8_t *buf, size_t size)
{
    struct _hs_hid_darwin *hid = port->u.hid;
    uint8_t report;
    kern_return_t kret;

    if (hid->device_removed)
        return hs_error(HS_ERROR_IO, "Device '%s' was removed", port->path);

    if (size < 2)
        return 0;

    report = buf[0];
    if (!report) {
        buf++;
        size--;
    }

    /* FIXME: find a way drop out of IOHIDDeviceSetReport() after a reasonable time, because
       IOHIDDeviceSetReportWithCallback() is broken. Perhaps we can open the device twice and
       close the write side to drop out of IOHIDDeviceSetReport() after a few seconds? Or maybe
       we can call IOHIDDeviceSetReport() in another thread and kill it, but I don't trust OSX
       to behave well in that case. The HID API does like to crash OSX for no reason. */
    kret = IOHIDDeviceSetReport(hid->hid_ref, type, report, buf, (CFIndex)size);
    if (kret != kIOReturnSuccess)
        return hs_error(HS_ERROR_IO, "I/O error while writing to '%s'", port->path);

    return (ssize_t)size + !report;
}

ssize_t hs_hid_write(hs_port *port, const uint8_t *buf, size_t size)
{
    assert(port);
    assert(port->type == HS_DEVICE_TYPE_HID);
    assert(port->mode & HS_PORT_MODE_WRITE);
    assert(buf);

    return send_report(port, kIOHIDReportTypeOutput, buf, size);
}

ssize_t hs_hid_get_feature_report(hs_port *port, uint8_t report_id, uint8_t *buf, size_t size)
{
    assert(port);
    assert(port->type == HS_DEVICE_TYPE_HID);
    assert(port->mode & HS_PORT_MODE_READ);
    assert(buf);
    assert(size);

    struct _hs_hid_darwin *hid = port->u.hid;
    CFIndex len;
    kern_return_t kret;

    if (hid->device_removed)
        return hs_error(HS_ERROR_IO, "Device '%s' was removed", port->path);

    len = (CFIndex)size - 1;
    kret = IOHIDDeviceGetReport(hid->hid_ref, kIOHIDReportTypeFeature, report_id,
                                buf + 1, &len);
    if (kret != kIOReturnSuccess)
        return hs_error(HS_ERROR_IO, "IOHIDDeviceGetReport() failed on '%s'", port->path);

    buf[0] = report_id;
    return (ssize_t)len;
}

ssize_t hs_hid_send_feature_report(hs_port *port, const uint8_t *buf, size_t size)
{
    assert(port);
    assert(port->type == HS_DEVICE_TYPE_HID);
    assert(port->mode & HS_PORT_MODE_WRITE);
    assert(buf);

    return send_report(port, kIOHIDReportTypeFeature, buf, size);
}

// monitor_darwin.c
// ------------------------------------

// #include "common_priv.h"
#include <CoreFoundation/CFRunLoop.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/hid/IOHIDDevice.h>
#include <mach/mach.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
// #include "device_priv.h"
// #include "match_priv.h"
// #include "monitor_priv.h"
// #include "platform.h"

struct hs_monitor {
    _hs_match_helper match_helper;
    _hs_htable devices;

    IONotificationPortRef notify_port;
    int kqfd;
    mach_port_t port_set;
    bool started;

    io_iterator_t iterators[8];
    unsigned int iterator_count;
    int notify_ret;

    hs_enumerate_func *callback;
    void *callback_udata;
};

struct device_class {
    hs_device_type type;
    const char *stacks[3];
};

struct service_aggregate {
    io_service_t dev_service;
    io_service_t iface_service;
    io_service_t usb_service;
};

static struct device_class device_classes[] = {
    {HS_DEVICE_TYPE_HID,    {"IOHIDDevice",       "IOUSBHostHIDDevice", "AppleUserHIDDevice"}},
    {HS_DEVICE_TYPE_SERIAL, {"IOSerialBSDClient", "IOSerialBSDClient",  "IOSerialBSDClient"}}
};

static int get_stack_version()
{
    static bool init;
    static int stack_version;

    if (!init) {
        uint32_t version = hs_darwin_version();

        if (version >= 190000) {
            stack_version = 2;
        } else if (version >= 150000) {
            stack_version = 1;
        } else {
            stack_version = 0;
        }

        init = true;
    }

    return stack_version;
}

static int get_ioregistry_value_string(io_service_t service, CFStringRef prop, char **rs)
{
    CFTypeRef data;
    CFIndex size;
    char *s;
    int r;

    data = IORegistryEntryCreateCFProperty(service, prop, kCFAllocatorDefault, 0);
    if (!data || CFGetTypeID(data) != CFStringGetTypeID()) {
        r = 0;
        goto cleanup;
    }

    size = CFStringGetMaximumSizeForEncoding(CFStringGetLength((CFStringRef)data),
                                             kCFStringEncodingUTF8) + 1;

    s = (char *)malloc((size_t)size);
    if (!s) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto cleanup;
    }

    r = CFStringGetCString((CFStringRef)data, s, size, kCFStringEncodingUTF8);
    if (!r) {
        r = 0;
        goto cleanup;
    }

    *rs = s;
    r = 1;
cleanup:
    if (data)
        CFRelease(data);
    return r;
}

static bool get_ioregistry_value_number(io_service_t service, CFStringRef prop, CFNumberType type,
                                        void *rn)
{
    CFTypeRef data;
    bool r;

    data = IORegistryEntryCreateCFProperty(service, prop, kCFAllocatorDefault, 0);
    if (!data || CFGetTypeID(data) != CFNumberGetTypeID()) {
        r = false;
        goto cleanup;
    }

    r = CFNumberGetValue((CFNumberRef)data, type, rn);
cleanup:
    if (data)
        CFRelease(data);
    return r;
}

static int get_ioregistry_entry_path(io_service_t service, char **rpath)
{
    io_string_t buf;
    char *path;
    kern_return_t kret;

    kret = IORegistryEntryGetPath(service, kIOServicePlane, buf);
    if (kret != kIOReturnSuccess) {
        hs_log(HS_LOG_DEBUG, "IORegistryEntryGetPath() failed with code %d", kret);
        return 0;
    }

    path = strdup(buf);
    if (!path)
        return hs_error(HS_ERROR_MEMORY, NULL);

    *rpath = path;
    return 1;
}

static void clear_iterator(io_iterator_t it)
{
    io_object_t object;
    while ((object = IOIteratorNext(it)))
        IOObjectRelease(object);
}

static int find_device_node(struct service_aggregate *agg, hs_device *dev)
{
    int r;

    if (IOObjectConformsTo(agg->dev_service, "IOSerialBSDClient")) {
        dev->type = HS_DEVICE_TYPE_SERIAL;

        r = get_ioregistry_value_string(agg->dev_service, CFSTR("IOCalloutDevice"), &dev->path);
        if (!r)
            hs_log(HS_LOG_WARNING, "Serial device does not have property 'IOCalloutDevice'");
    } else if (IOObjectConformsTo(agg->dev_service, "IOHIDDevice")) {
        dev->type = HS_DEVICE_TYPE_HID;

        r = get_ioregistry_entry_path(agg->dev_service, &dev->path);
    } else {
        hs_log(HS_LOG_WARNING, "Cannot find device node for unknown device entry class");
        r = 0;
    }

    return r;
}

static int build_location_string(uint8_t ports[], unsigned int depth, char **rpath)
{
    char buf[256];
    char *ptr;
    size_t size;
    char *path;
    int r;

    ptr = buf;
    size = sizeof(buf);

    strcpy(buf, "usb");
    ptr += strlen(buf);
    size -= (size_t)(ptr - buf);

    for (unsigned int i = 0; i < depth; i++) {
        r = snprintf(ptr, size, "-%hhu", ports[i]);
        assert(r >= 2 && (size_t)r < size);

        ptr += r;
        size -= (size_t)r;
    }

    path = strdup(buf);
    if (!path)
        return hs_error(HS_ERROR_MEMORY, NULL);

    *rpath = path;
    return 0;
}

static io_service_t get_parent_and_release(io_service_t service, const io_name_t plane)
{
    io_service_t parent;
    kern_return_t kret;

    kret = IORegistryEntryGetParentEntry(service, plane, &parent);
    IOObjectRelease(service);
    if (kret != kIOReturnSuccess)
        return 0;

    return parent;
}

static int resolve_device_location(io_service_t usb_service, char **rlocation)
{
    uint32_t location_id;
    uint8_t ports[16];
    unsigned int depth;
    int r;

    r = get_ioregistry_value_number(usb_service, CFSTR("locationID"), kCFNumberSInt32Type,
                                    &location_id);
    if (!r) {
        hs_log(HS_LOG_WARNING, "Ignoring device without 'locationID' property");
        return 0;
    }

    ports[0] = location_id >> 24;
    for (depth = 0; depth <= 5 && ports[depth]; depth++)
        ports[depth + 1] = (location_id >> (20 - depth * 4)) & 0xF;

    r = build_location_string(ports, depth, rlocation);
    if (r < 0)
        return r;

    return 1;
}

static io_service_t find_conforming_parent(io_service_t service, const char *cls)
{
    IOObjectRetain(service);
    do {
        service = get_parent_and_release(service, kIOServicePlane);
    } while (service && !IOObjectConformsTo(service, cls));

    return service;
}

static int fill_device_details(struct service_aggregate *agg, hs_device *dev)
{
    uint64_t session;
    int r;

#define GET_MANDATORY_PROPERTY_NUMBER(service, key, type, var) \
        r = get_ioregistry_value_number((service), CFSTR(key), (type), (var)); \
        if (!r) { \
            hs_log(HS_LOG_WARNING, "Missing property '%s', ignoring device", (key)); \
            return 0; \
        }
#define GET_OPTIONAL_PROPERTY_STRING(service, key, var) \
        r = get_ioregistry_value_string((service), CFSTR(key), (var)); \
        if (r < 0) \
            return r;

    GET_MANDATORY_PROPERTY_NUMBER(agg->usb_service, "sessionID", kCFNumberSInt64Type, &session);
    GET_MANDATORY_PROPERTY_NUMBER(agg->usb_service, "idVendor", kCFNumberSInt16Type, &dev->vid);
    GET_MANDATORY_PROPERTY_NUMBER(agg->usb_service, "idProduct", kCFNumberSInt16Type, &dev->pid);
    GET_MANDATORY_PROPERTY_NUMBER(agg->usb_service, "bcdDevice", kCFNumberSInt16Type,
                                  &dev->bcd_device);
    GET_MANDATORY_PROPERTY_NUMBER(agg->iface_service, "bInterfaceNumber", kCFNumberSInt8Type,
                                  &dev->iface_number);

    GET_OPTIONAL_PROPERTY_STRING(agg->usb_service, "USB Vendor Name", &dev->manufacturer_string);
    GET_OPTIONAL_PROPERTY_STRING(agg->usb_service, "USB Product Name", &dev->product_string);
    GET_OPTIONAL_PROPERTY_STRING(agg->usb_service, "USB Serial Number", &dev->serial_number_string);

#undef GET_MANDATORY_PROPERTY_NUMBER
#undef GET_OPTIONAL_PROPERTY_STRING

    r = _hs_asprintf(&dev->key, "%" PRIx64, session);
    if (r < 0)
        return hs_error(HS_ERROR_MEMORY, NULL);

    return 1;
}

static void fill_hid_properties(struct service_aggregate *agg, hs_device *dev)
{
    bool success = true;

    success &= get_ioregistry_value_number(agg->dev_service, CFSTR("PrimaryUsagePage"),
                                           kCFNumberSInt16Type, &dev->u.hid.usage_page);
    success &= get_ioregistry_value_number(agg->dev_service, CFSTR("PrimaryUsage"),
                                           kCFNumberSInt16Type, &dev->u.hid.usage);

    if (!success)
        hs_log(HS_LOG_WARNING, "Invalid HID values for '%s", dev->path);
}

static int process_darwin_device(io_service_t service, hs_device **rdev)
{
    struct service_aggregate agg = {0};
    hs_device *dev = NULL;
    int r;

    agg.dev_service = service;
    agg.iface_service = find_conforming_parent(agg.dev_service, "IOUSBInterface");
    agg.usb_service = find_conforming_parent(agg.iface_service, "IOUSBDevice");
    if (!agg.iface_service || !agg.usb_service) {
        r = 0;
        goto cleanup;
    }

    dev = (hs_device *)calloc(1, sizeof(*dev));
    if (!dev) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto cleanup;
    }
    dev->refcount = 1;
    dev->status = HS_DEVICE_STATUS_ONLINE;

    r = find_device_node(&agg, dev);
    if (r <= 0)
        goto cleanup;

    r = fill_device_details(&agg, dev);
    if (r <= 0)
        goto cleanup;

    if (dev->type == HS_DEVICE_TYPE_HID)
        fill_hid_properties(&agg, dev);

    r = resolve_device_location(agg.usb_service, &dev->location);
    if (r <= 0)
        goto cleanup;

    *rdev = dev;
    dev = NULL;
    r = 1;

cleanup:
    hs_device_unref(dev);
    if (agg.usb_service)
        IOObjectRelease(agg.usb_service);
    if (agg.iface_service)
        IOObjectRelease(agg.iface_service);
    return r;
}

static int process_iterator_devices(io_iterator_t it, const _hs_match_helper *match_helper,
                                    hs_enumerate_func *f, void *udata)
{
    io_service_t service;

    while ((service = IOIteratorNext(it))) {
        hs_device *dev;
        int r;

        r = process_darwin_device(service, &dev);
        IOObjectRelease(service);
        if (r < 0)
            return r;
        if (!r)
            continue;

        if (_hs_match_helper_match(match_helper, dev, &dev->match_udata)) {
            r = (*f)(dev, udata);
            hs_device_unref(dev);
            if (r)
                return r;
        } else {
            hs_device_unref(dev);
        }
    }

    return 0;
}

static int attached_callback(hs_device *dev, void *udata)
{
    hs_monitor *monitor = (hs_monitor *)udata;

    if (!_hs_match_helper_match(&monitor->match_helper, dev, &dev->match_udata))
        return 0;
    return _hs_monitor_add(&monitor->devices, dev, monitor->callback, monitor->callback_udata);
}

static void darwin_devices_attached(void *udata, io_iterator_t it)
{
    hs_monitor *monitor = (hs_monitor *)udata;

    monitor->notify_ret = process_iterator_devices(it, &monitor->match_helper,
                                                   attached_callback, monitor);
}

static void darwin_devices_detached(void *udata, io_iterator_t it)
{
    hs_monitor *monitor = (hs_monitor *)udata;

    io_service_t service;
    while ((service = IOIteratorNext(it))) {
        uint64_t session;
        int r;

        r = get_ioregistry_value_number(service, CFSTR("sessionID"), kCFNumberSInt64Type, &session);
        if (r) {
            char key[32];

            sprintf(key, "%" PRIx64, session);
            _hs_monitor_remove(&monitor->devices, key, monitor->callback, monitor->callback_udata);
        }

        IOObjectRelease(service);
    }
}

struct enumerate_enumerate_context {
    hs_enumerate_func *f;
    void *udata;
};

static int enumerate_enumerate_callback(hs_device *dev, void *udata)
{
    struct enumerate_enumerate_context *ctx = (struct enumerate_enumerate_context *)udata;

    _hs_device_log(dev, "Enumerate");
    return (*ctx->f)(dev, ctx->udata);
}

int hs_enumerate(const hs_match_spec *matches, unsigned int count, hs_enumerate_func *f, void *udata)
{
    assert(f);

    _hs_match_helper match_helper;
    struct enumerate_enumerate_context ctx;
    io_iterator_t it = 0;
    kern_return_t kret;
    int r;

    r = _hs_match_helper_init(&match_helper, matches, count);
    if (r < 0)
        goto cleanup;

    ctx.f = f;
    ctx.udata = udata;

    for (unsigned int i = 0; i < _HS_COUNTOF(device_classes); i++) {
        if (_hs_match_helper_has_type(&match_helper, device_classes[i].type)) {
            const char *cls = device_classes[i].stacks[get_stack_version()];

            kret = IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching(cls), &it);
            if (kret != kIOReturnSuccess) {
                r = hs_error(HS_ERROR_SYSTEM, "IOServiceGetMatchingServices('%s') failed", cls);
                goto cleanup;
            }

            r = process_iterator_devices(it, &match_helper, enumerate_enumerate_callback, &ctx);
            if (r)
                goto cleanup;

            IOObjectRelease(it);
            it = 0;
        }
    }

    r = 0;
cleanup:
    if (it) {
        clear_iterator(it);
        IOObjectRelease(it);
    }
    _hs_match_helper_release(&match_helper);
    return r;
}

static int add_notification(hs_monitor *monitor, const char *cls, const io_name_t type,
                            IOServiceMatchingCallback f, io_iterator_t *rit)
{
    io_iterator_t it;
    kern_return_t kret;

    kret = IOServiceAddMatchingNotification(monitor->notify_port, type, IOServiceMatching(cls),
                                            f, monitor, &it);
    if (kret != kIOReturnSuccess)
        return hs_error(HS_ERROR_SYSTEM, "IOServiceAddMatchingNotification('%s') failed", cls);

    assert(monitor->iterator_count < _HS_COUNTOF(monitor->iterators));
    monitor->iterators[monitor->iterator_count++] = it;
    *rit = it;

    return 0;
}

int hs_monitor_new(const hs_match_spec *matches, unsigned int count, hs_monitor **rmonitor)
{
    assert(rmonitor);

    hs_monitor *monitor = NULL;
    struct kevent kev;
    const struct timespec ts = {0};
    kern_return_t kret;
    int r;

    monitor = (hs_monitor *)calloc(1, sizeof(*monitor));
    if (!monitor) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto error;
    }

    monitor->kqfd = -1;

    r = _hs_match_helper_init(&monitor->match_helper, matches, count);
    if (r < 0)
        goto error;

    r = _hs_htable_init(&monitor->devices, 64);
    if (r < 0)
        goto error;

    monitor->notify_port = IONotificationPortCreate(kIOMasterPortDefault);
    if (!monitor->notify_port) {
        r = hs_error(HS_ERROR_SYSTEM, "IONotificationPortCreate() failed");
        goto error;
    }

    monitor->kqfd = kqueue();
    if (monitor->kqfd < 0) {
        r = hs_error(HS_ERROR_SYSTEM, "kqueue() failed: %s", strerror(errno));
        goto error;
    }

    kret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_PORT_SET, &monitor->port_set);
    if (kret != KERN_SUCCESS) {
        r = hs_error(HS_ERROR_SYSTEM, "mach_port_allocate() failed");
        goto error;
    }

    kret = mach_port_insert_member(mach_task_self(), IONotificationPortGetMachPort(monitor->notify_port),
                                   monitor->port_set);
    if (kret != KERN_SUCCESS) {
        r = hs_error(HS_ERROR_SYSTEM, "mach_port_insert_member() failed");
        goto error;
    }

    EV_SET(&kev, monitor->port_set, EVFILT_MACHPORT, EV_ADD, 0, 0, NULL);

    r = kevent(monitor->kqfd, &kev, 1, NULL, 0, &ts);
    if (r < 0) {
        r = hs_error(HS_ERROR_SYSTEM, "kevent() failed: %d", errno);
        goto error;
    }

    *rmonitor = monitor;
    return 0;

error:
    hs_monitor_free(monitor);
    return r;
}

void hs_monitor_free(hs_monitor *monitor)
{
    if (monitor) {
        for (unsigned int i = 0; i < monitor->iterator_count; i++) {
            clear_iterator(monitor->iterators[i]);
            IOObjectRelease(monitor->iterators[i]);
        }

        if (monitor->port_set)
            mach_port_deallocate(mach_task_self(), monitor->port_set);
        if (monitor->notify_port)
            IONotificationPortDestroy(monitor->notify_port);
        close(monitor->kqfd);

        _hs_monitor_clear_devices(&monitor->devices);
        _hs_htable_release(&monitor->devices);
        _hs_match_helper_release(&monitor->match_helper);
    }

    free(monitor);
}

hs_handle hs_monitor_get_poll_handle(const hs_monitor *monitor)
{
    assert(monitor);
    return monitor->kqfd;
}

static int start_enumerate_callback(hs_device *dev, void *udata)
{
    hs_monitor *monitor = (hs_monitor *)udata;
    return _hs_monitor_add(&monitor->devices, dev, NULL, NULL);
}

int hs_monitor_start(hs_monitor *monitor)
{
    assert(monitor);

    io_iterator_t it;
    int r;

    if (monitor->started)
        return 0;

    for (unsigned int i = 0; i < _HS_COUNTOF(device_classes); i++) {
        if (_hs_match_helper_has_type(&monitor->match_helper, device_classes[i].type)) {
            const char *cls = device_classes[i].stacks[get_stack_version()];

            r = add_notification(monitor, cls, kIOFirstMatchNotification,
                                 darwin_devices_attached, &it);
            if (r < 0)
                goto error;

            r = process_iterator_devices(it, &monitor->match_helper, start_enumerate_callback, monitor);
            if (r < 0)
                goto error;
        }
    }

    r = add_notification(monitor, hs_version() >= 150000 ? "IOUSBHostDevice" : kIOUSBDeviceClassName,
                         kIOTerminatedNotification, darwin_devices_detached, &it);
    if (r < 0)
        goto error;
    clear_iterator(it);

    monitor->started = true;
    return 0;

error:
    hs_monitor_stop(monitor);
    return r;
}

void hs_monitor_stop(hs_monitor *monitor)
{
    assert(monitor);

    if (!monitor->started)
        return;

    _hs_monitor_clear_devices(&monitor->devices);
    for (unsigned int i = 0; i < monitor->iterator_count; i++) {
        clear_iterator(monitor->iterators[i]);
        IOObjectRelease(monitor->iterators[i]);
    }
    monitor->iterator_count = 0;

    monitor->started = false;
}

int hs_monitor_refresh(hs_monitor *monitor, hs_enumerate_func *f, void *udata)
{
    assert(monitor);

    struct kevent kev;
    const struct timespec ts = {0};
    int r;

    if (!monitor->started)
        return 0;

    r = kevent(monitor->kqfd, NULL, 0, &kev, 1, &ts);
    if (r < 0)
        return hs_error(HS_ERROR_SYSTEM, "kevent() failed: %s", strerror(errno));
    if (!r)
        return 0;
    assert(kev.filter == EVFILT_MACHPORT);

    monitor->callback = f;
    monitor->callback_udata = udata;

    r = 0;
    while (true) {
        struct {
            mach_msg_header_t header;
            uint8_t body[128];
        } msg;
        mach_msg_return_t mret;

        mret = mach_msg(&msg.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(msg),
                        monitor->port_set, 0, MACH_PORT_NULL);
        if (mret != MACH_MSG_SUCCESS) {
            if (mret == MACH_RCV_TIMED_OUT)
                break;

            r = hs_error(HS_ERROR_SYSTEM, "mach_msg() failed");
            break;
        }

        IODispatchCalloutFromMessage(NULL, &msg.header, monitor->notify_port);

        if (monitor->notify_ret) {
            r = monitor->notify_ret;
            monitor->notify_ret = 0;

            break;
        }
    }

    return r;
}

int hs_monitor_list(hs_monitor *monitor, hs_enumerate_func *f, void *udata)
{
    return _hs_monitor_list(&monitor->devices, f, udata);
}

// platform_darwin.c
// ------------------------------------

// #include "common_priv.h"
#include <mach/mach_time.h>
#include <sys/select.h>
#include <sys/utsname.h>
#include <time.h>
// #include "platform.h"

uint64_t hs_millis(void)
{
    static mach_timebase_info_data_t tb;
    if (!tb.numer)
        mach_timebase_info(&tb);

    return (uint64_t)mach_absolute_time() * tb.numer / tb.denom / 1000000;
}

void hs_delay(unsigned int ms)
{
    struct timespec t, rem;
    int r;

    t.tv_sec = (int)(ms / 1000);
    t.tv_nsec = (int)((ms % 1000) * 1000000);

    do {
        r = nanosleep(&t, &rem);
        if (r < 0) {
            if (errno != EINTR) {
                hs_error(HS_ERROR_SYSTEM, "nanosleep() failed: %s", strerror(errno));
                return;
            }

            t = rem;
        }
    } while (r);
}

int hs_poll(hs_poll_source *sources, unsigned int count, int timeout)
{
    assert(sources);
    assert(count);
    assert(count <= HS_POLL_MAX_SOURCES);

    fd_set fds;
    uint64_t start;
    int maxfd, r;

    FD_ZERO(&fds);
    maxfd = 0;
    for (unsigned int i = 0; i < count; i++) {
        if (sources[i].desc >= FD_SETSIZE) {
            for (unsigned int j = i; j < count; j++)
                sources[j].ready = 0;

            return hs_error(HS_ERROR_SYSTEM, "Cannot select() on descriptor %d (too big)",
                            sources[i].desc);
        }

        FD_SET(sources[i].desc, &fds);
        sources[i].ready = 0;

        if (sources[i].desc > maxfd)
            maxfd = sources[i].desc;
    }

    start = hs_millis();
restart:
    if (timeout >= 0) {
        int adjusted_timeout;
        struct timeval tv;

        adjusted_timeout = hs_adjust_timeout(timeout, start);
        tv.tv_sec = adjusted_timeout / 1000;
        tv.tv_usec = (adjusted_timeout % 1000) * 1000;

        r = select(maxfd + 1, &fds, NULL, NULL, &tv);
    } else {
        r = select(maxfd + 1, &fds, NULL, NULL, NULL);
    }
    if (r < 0) {
        if (errno == EINTR)
            goto restart;
        return hs_error(HS_ERROR_SYSTEM, "poll() failed: %s", strerror(errno));
    }
    if (!r)
        return 0;

    for (unsigned int i = 0; i < count; i++)
        sources[i].ready = !!FD_ISSET(sources[i].desc, &fds);

    return r;
}

uint32_t hs_darwin_version(void)
{
    static uint32_t version;

    if (!version) {
        struct utsname name;
        uint32_t major = 0, minor = 0, release = 0;

        uname(&name);
        sscanf(name.release, "%u.%u.%u", &major, &minor, &release);

        version = major * 10000 + minor * 100 + release;
    }

    return version;
}

// serial_posix.c
// ------------------------------------

// #include "common_priv.h"
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
// #include "device_priv.h"
// #include "platform.h"
// #include "serial.h"

int hs_serial_set_config(hs_port *port, const hs_serial_config *config)
{
    assert(port);
    assert(config);

    struct termios tio;
    int modem_bits;
    int r;

    r = tcgetattr(port->u.file.fd, &tio);
    if (r < 0)
        return hs_error(HS_ERROR_SYSTEM, "Unable to get serial port settings from '%s': %s",
                        port->path, strerror(errno));
    r = ioctl(port->u.file.fd, TIOCMGET, &modem_bits);
    if (r < 0)
        return hs_error(HS_ERROR_SYSTEM, "Unable to get modem bits from '%s': %s",
                        port->path, strerror(errno));

    if (config->baudrate) {
        speed_t std_baudrate;

        switch (config->baudrate) {
            case 110: { std_baudrate = B110; } break;
            case 134: { std_baudrate = B134; } break;
            case 150: { std_baudrate = B150; } break;
            case 200: { std_baudrate = B200; } break;
            case 300: { std_baudrate = B300; } break;
            case 600: { std_baudrate = B600; } break;
            case 1200: { std_baudrate = B1200; } break;
            case 1800: { std_baudrate = B1800; } break;
            case 2400: { std_baudrate = B2400; } break;
            case 4800: { std_baudrate = B4800; } break;
            case 9600: { std_baudrate = B9600; } break;
            case 19200: { std_baudrate = B19200; } break;
            case 38400: { std_baudrate = B38400; } break;
            case 57600: { std_baudrate = B57600; } break;
            case 115200: { std_baudrate = B115200; } break;
            case 230400: { std_baudrate = B230400; } break;

            default: {
                return hs_error(HS_ERROR_SYSTEM, "Unsupported baud rate value: %u",
                                config->baudrate);
            } break;
        }

        cfsetispeed(&tio, std_baudrate);
        cfsetospeed(&tio, std_baudrate);
    }

    if (config->databits) {
        tio.c_cflag &= (unsigned int)~CSIZE;

        switch (config->databits) {
            case 5: { tio.c_cflag |= CS5; } break;
            case 6: { tio.c_cflag |= CS6; } break;
            case 7: { tio.c_cflag |= CS7; } break;
            case 8: { tio.c_cflag |= CS8; } break;

            default: {
                return hs_error(HS_ERROR_SYSTEM, "Invalid data bits setting: %u",
                                config->databits);
            } break;
        }
    }

    if (config->stopbits) {
        tio.c_cflag &= (unsigned int)~CSTOPB;

        switch (config->stopbits) {
            case 1: {} break;
            case 2: { tio.c_cflag |= CSTOPB; } break;

            default: {
                return hs_error(HS_ERROR_SYSTEM, "Invalid stop bits setting: %u",
                                config->stopbits);
            } break;
        }
    }

    if (config->parity) {
        tio.c_cflag &= (unsigned int)~(PARENB | PARODD);
#ifdef CMSPAR
        tio.c_cflag &= (unsigned int)~CMSPAR;
#endif

        switch (config->parity) {
            case HS_SERIAL_CONFIG_PARITY_OFF: {} break;
            case HS_SERIAL_CONFIG_PARITY_EVEN: { tio.c_cflag |= PARENB; } break;
            case HS_SERIAL_CONFIG_PARITY_ODD: { tio.c_cflag |= PARENB | PARODD; } break;
#ifdef CMSPAR
            case HS_SERIAL_CONFIG_PARITY_SPACE: { tio.c_cflag |= PARENB | CMSPAR; } break;
            case HS_SERIAL_CONFIG_PARITY_MARK: { tio.c_cflag |= PARENB | PARODD | CMSPAR; } break;
#else

            case HS_SERIAL_CONFIG_PARITY_MARK:
            case HS_SERIAL_CONFIG_PARITY_SPACE: {
                return hs_error(HS_ERROR_SYSTEM, "Mark/space parity is not supported");
            } break;
#endif
            default: {
                return hs_error(HS_ERROR_SYSTEM, "Invalid parity setting: %d", config->parity);
            } break;
        }
    }

    if (config->rts) {
        tio.c_cflag &= (unsigned int)~CRTSCTS;
        modem_bits &= ~TIOCM_RTS;

        switch (config->rts) {
            case HS_SERIAL_CONFIG_RTS_OFF: {} break;
            case HS_SERIAL_CONFIG_RTS_ON: { modem_bits |= TIOCM_RTS; } break;
            case HS_SERIAL_CONFIG_RTS_FLOW: { tio.c_cflag |= CRTSCTS; } break;

            default: {
                return hs_error(HS_ERROR_SYSTEM, "Invalid RTS setting: %d", config->rts);
            } break;
        }
    }

    switch (config->dtr) {
        case 0: {} break;
        case HS_SERIAL_CONFIG_DTR_OFF: { modem_bits &= ~TIOCM_DTR; } break;
        case HS_SERIAL_CONFIG_DTR_ON: { modem_bits |= TIOCM_DTR; } break;

        default: {
            return hs_error(HS_ERROR_SYSTEM, "Invalid DTR setting: %d", config->dtr);
        } break;
    }

    if (config->xonxoff) {
        tio.c_iflag &= (unsigned int)~(IXON | IXOFF | IXANY);

        switch (config->xonxoff) {
            case HS_SERIAL_CONFIG_XONXOFF_OFF: {} break;
            case HS_SERIAL_CONFIG_XONXOFF_IN: { tio.c_iflag |= IXOFF; } break;
            case HS_SERIAL_CONFIG_XONXOFF_OUT: { tio.c_iflag |= IXON | IXANY; } break;
            case HS_SERIAL_CONFIG_XONXOFF_INOUT: { tio.c_iflag |= IXOFF | IXON | IXANY; } break;

            default: {
                return hs_error(HS_ERROR_SYSTEM, "Invalid XON/XOFF setting: %d", config->xonxoff);
            } break;
        }
    }

    r = ioctl(port->u.file.fd, TIOCMSET, &modem_bits);
    if (r < 0)
        return hs_error(HS_ERROR_SYSTEM, "Unable to set modem bits of '%s': %s",
                        port->path, strerror(errno));
    r = tcsetattr(port->u.file.fd, TCSANOW, &tio);
    if (r < 0)
        return hs_error(HS_ERROR_SYSTEM, "Unable to change serial port settings of '%s': %s",
                        port->path, strerror(errno));

    return 0;
}

int hs_serial_get_config(hs_port *port, hs_serial_config *config)
{
    assert(port);

    struct termios tio;
    int modem_bits;
    int r;

    r = tcgetattr(port->u.file.fd, &tio);
    if (r < 0)
        return hs_error(HS_ERROR_SYSTEM, "Unable to read port settings from '%s': %s",
                        port->path, strerror(errno));
    r = ioctl(port->u.file.fd, TIOCMGET, &modem_bits);
    if (r < 0)
        return hs_error(HS_ERROR_SYSTEM, "Unable to get modem bits from '%s': %s",
                        port->path, strerror(errno));

    /* 0 is the INVALID value for all parameters, we keep that value if we can't interpret
       a termios value (only a cross-platform subset of it is exposed in hs_serial_config). */
    memset(config, 0, sizeof(*config));

    switch (cfgetispeed(&tio)) {
        case B110: { config->baudrate = 110; } break;
        case B134: { config->baudrate = 134; } break;
        case B150: { config->baudrate = 150; } break;
        case B200: { config->baudrate = 200; } break;
        case B300: { config->baudrate = 300; } break;
        case B600: { config->baudrate = 600; } break;
        case B1200: { config->baudrate = 1200; } break;
        case B1800: { config->baudrate = 1800; } break;
        case B2400: { config->baudrate = 2400; } break;
        case B4800: { config->baudrate = 4800; } break;
        case B9600: { config->baudrate = 9600; } break;
        case B19200: { config->baudrate = 19200; } break;
        case B38400: { config->baudrate = 38400; } break;
        case B57600: { config->baudrate = 57600; } break;
        case B115200: { config->baudrate = 115200; } break;
        case B230400: { config->baudrate = 230400; } break;
    }

    switch (tio.c_cflag & CSIZE) {
        case CS5: { config->databits = 5; } break;
        case CS6: { config->databits = 6; } break;
        case CS7: { config->databits = 7; } break;
        case CS8: { config->databits = 8; } break;
    }

    if (tio.c_cflag & CSTOPB) {
        config->stopbits = 2;
    } else {
        config->stopbits = 1;
    }

    // FIXME: should we detect IGNPAR here?
    if (tio.c_cflag & PARENB) {
#ifdef CMSPAR
        switch (tio.c_cflag & (PARODD | CMSPAR)) {
#else
        switch (tio.c_cflag & PARODD) {
#endif
            case 0: { config->parity = HS_SERIAL_CONFIG_PARITY_EVEN; } break;
            case PARODD: { config->parity = HS_SERIAL_CONFIG_PARITY_ODD; } break;
#ifdef CMSPAR
            case CMSPAR: { config->parity = HS_SERIAL_CONFIG_PARITY_SPACE; } break;
            case CMSPAR | PARODD: { config->parity = HS_SERIAL_CONFIG_PARITY_MARK; } break;
#endif
        }
    } else {
        config->parity = HS_SERIAL_CONFIG_PARITY_OFF;
    }

    if (tio.c_cflag & CRTSCTS) {
        config->rts = HS_SERIAL_CONFIG_RTS_FLOW;
    } else if (modem_bits & TIOCM_RTS) {
        config->rts = HS_SERIAL_CONFIG_RTS_ON;
    } else {
        config->rts = HS_SERIAL_CONFIG_RTS_OFF;
    }

    if (modem_bits & TIOCM_DTR) {
        config->dtr = HS_SERIAL_CONFIG_DTR_ON;
    } else {
        config->dtr = HS_SERIAL_CONFIG_DTR_OFF;
    }

    switch (tio.c_iflag & (IXON | IXOFF)) {
        case 0: { config->xonxoff = HS_SERIAL_CONFIG_XONXOFF_OFF; } break;
        case IXOFF: { config->xonxoff = HS_SERIAL_CONFIG_XONXOFF_IN; } break;
        case IXON: { config->xonxoff = HS_SERIAL_CONFIG_XONXOFF_OUT; } break;
        case IXOFF | IXON: { config->xonxoff = HS_SERIAL_CONFIG_XONXOFF_INOUT; } break;
    }

    return 0;
}

ssize_t hs_serial_read(hs_port *port, uint8_t *buf, size_t size, int timeout)
{
    assert(port);
    assert(port->type == HS_DEVICE_TYPE_SERIAL);
    assert(port->mode & HS_PORT_MODE_READ);
    assert(buf);
    assert(size);

    ssize_t r;

    if (timeout) {
        struct pollfd pfd;
        uint64_t start;

        pfd.events = POLLIN;
        pfd.fd = port->u.file.fd;

        start = hs_millis();
restart:
        r = poll(&pfd, 1, hs_adjust_timeout(timeout, start));
        if (r < 0) {
            if (errno == EINTR)
                goto restart;

            return hs_error(HS_ERROR_IO, "I/O error while reading from '%s': %s", port->path,
                            strerror(errno));
        }
        if (!r)
            return 0;
    }

    r = read(port->u.file.fd, buf, size);
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;

        return hs_error(HS_ERROR_IO, "I/O error while reading from '%s': %s", port->path,
                        strerror(errno));
    }

    return r;
}

ssize_t hs_serial_write(hs_port *port, const uint8_t *buf, size_t size, int timeout)
{
    assert(port);
    assert(port->type == HS_DEVICE_TYPE_SERIAL);
    assert(port->mode & HS_PORT_MODE_WRITE);
    assert(buf);

    struct pollfd pfd;
    uint64_t start;
    int adjusted_timeout;
    size_t written;

    pfd.events = POLLOUT;
    pfd.fd = port->u.file.fd;

    start = hs_millis();
    adjusted_timeout = timeout;

    written = 0;
    do {
        ssize_t r;

        r = poll(&pfd, 1, adjusted_timeout);
        if (r < 0) {
            if (errno == EINTR)
                continue;

            return hs_error(HS_ERROR_IO, "I/O error while writing to '%s': %s", port->path,
                            strerror(errno));
        }
        if (!r)
            break;

        r = write(port->u.file.fd, buf + written, size - written);
        if (r < 0) {
            if (errno == EINTR)
                continue;

            return hs_error(HS_ERROR_IO, "I/O error while writing to '%s': %s", port->path,
                            strerror(errno));
        }
        written += (size_t)r;

        adjusted_timeout = hs_adjust_timeout(timeout, start);
    } while (written < size && adjusted_timeout);

    return (ssize_t)written;
}

    #elif defined(__linux__)
// device_posix.c
// ------------------------------------

// #include "common_priv.h"
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
// #include "device_priv.h"
// #include "platform.h"

int _hs_open_file_port(hs_device *dev, hs_port_mode mode, hs_port **rport)
{
    hs_port *port;
#ifdef __APPLE__
    unsigned int retry = 4;
#endif
    int fd_flags;
    int r;

    port = (hs_port *)calloc(1, sizeof(*port));
    if (!port) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto error;
    }
    port->type = dev->type;
    port->u.file.fd = -1;

    port->mode = mode;
    port->path = dev->path;
    port->dev = hs_device_ref(dev);

    fd_flags = O_CLOEXEC | O_NOCTTY | O_NONBLOCK;
    switch (mode) {
        case HS_PORT_MODE_READ: { fd_flags |= O_RDONLY; } break;
        case HS_PORT_MODE_WRITE: { fd_flags |= O_WRONLY; } break;
        case HS_PORT_MODE_RW: { fd_flags |= O_RDWR; } break;
    }

restart:
    port->u.file.fd = open(dev->path, fd_flags);
    if (port->u.file.fd < 0) {
        switch (errno) {
            case EINTR: {
                goto restart;
            } break;

            case EACCES: {
                r = hs_error(HS_ERROR_ACCESS, "Permission denied for device '%s'", dev->path);
            } break;
            case EIO:
            case ENXIO:
            case ENODEV: {
                r = hs_error(HS_ERROR_IO, "I/O error while opening device '%s'", dev->path);
            } break;
            case ENOENT:
            case ENOTDIR: {
                r = hs_error(HS_ERROR_NOT_FOUND, "Device '%s' not found", dev->path);
            } break;

#ifdef __APPLE__
            /* On El Capitan (and maybe before), the open fails for some time (around 40 - 50 ms
               on my computer) after the device notification. */
            case EBUSY: {
                if (retry--) {
                    usleep(20000);
                    goto restart;
                }
            } // fallthrough
#endif

            default: {
                r = hs_error(HS_ERROR_SYSTEM, "open('%s') failed: %s", dev->path, strerror(errno));
            } break;
        }
        goto error;
    }

    if (dev->type == HS_DEVICE_TYPE_SERIAL) {
        struct termios tio;
        int modem_bits;

        r = tcgetattr(port->u.file.fd, &tio);
        if (r < 0) {
            r = hs_error(HS_ERROR_SYSTEM, "tcgetattr() failed on '%s': %s", dev->path,
                         strerror(errno));
            goto error;
        }

        /* Use raw I/O and sane settings, set DTR by default even on platforms that don't
           enforce that. */
        cfmakeraw(&tio);
        tio.c_cc[VMIN] = 0;
        tio.c_cc[VTIME] = 0;
        tio.c_cflag |= CLOCAL | CREAD | HUPCL;
        modem_bits = TIOCM_DTR;

        r = tcsetattr(port->u.file.fd, TCSANOW, &tio);
        if (r < 0) {
            r = hs_error(HS_ERROR_SYSTEM, "tcsetattr() failed on '%s': %s", dev->path,
                         strerror(errno));
            goto error;
        }
        r = ioctl(port->u.file.fd, TIOCMBIS, &modem_bits);
        if (r < 0) {
            r = hs_error(HS_ERROR_SYSTEM, "ioctl(TIOCMBIS, TIOCM_DTR) failed on '%s': %s",
                         dev->path, strerror(errno));
            goto error;
        }
        r = tcflush(port->u.file.fd, TCIFLUSH);
        if (r < 0) {
            r = hs_error(HS_ERROR_SYSTEM, "tcflush(TCIFLUSH) failed on '%s': %s",
                         dev->path, strerror(errno));
            goto error;
        }
    }
#ifdef __linux__
    else if (dev->type == HS_DEVICE_TYPE_HID) {
        port->u.file.numbered_hid_reports = dev->u.hid.numbered_reports;
    }
#endif

    *rport = port;
    return 0;

error:
    hs_port_close(port);
    return r;
}

void _hs_close_file_port(hs_port *port)
{
    if (port) {
#ifdef __linux__
        // Only used for hidraw to work around a bug on old kernels
        free(port->u.file.read_buf);
#endif

        close(port->u.file.fd);
        hs_device_unref(port->dev);
    }

    free(port);
}

hs_handle _hs_get_file_port_poll_handle(const hs_port *port)
{
    return port->u.file.fd;
}

// hid_linux.c
// ------------------------------------

// #include "common_priv.h"
#include <fcntl.h>
#include <linux/hidraw.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
// #include "device_priv.h"
// #include "hid.h"
// #include "platform.h"

static bool detect_kernel26_byte_bug()
{
    static bool init, bug;

    if (!init) {
        bug = hs_linux_version() >= 20628000 && hs_linux_version() < 20634000;
        init = true;
    }

    return bug;
}

ssize_t hs_hid_read(hs_port *port, uint8_t *buf, size_t size, int timeout)
{
    assert(port);
    assert(port->type == HS_DEVICE_TYPE_HID);
    assert(port->mode & HS_PORT_MODE_READ);
    assert(buf);
    assert(size);

    ssize_t r;

    if (timeout) {
        struct pollfd pfd;
        uint64_t start;

        pfd.events = POLLIN;
        pfd.fd = port->u.file.fd;

        start = hs_millis();
restart:
        r = poll(&pfd, 1, hs_adjust_timeout(timeout, start));
        if (r < 0) {
            if (errno == EINTR)
                goto restart;

            return hs_error(HS_ERROR_IO, "I/O error while reading from '%s': %s", port->path,
                            strerror(errno));
        }
        if (!r)
            return 0;
    }

    if (port->u.file.numbered_hid_reports) {
        /* Work around a hidraw bug introduced in Linux 2.6.28 and fixed in Linux 2.6.34, see
           https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/commit/?id=5a38f2c7c4dd53d5be097930902c108e362584a3 */
        if (detect_kernel26_byte_bug()) {
            if (size + 1 > port->u.file.read_buf_size) {
                free(port->u.file.read_buf);
                port->u.file.read_buf_size = 0;

                port->u.file.read_buf = (uint8_t *)malloc(size + 1);
                if (!port->u.file.read_buf)
                    return hs_error(HS_ERROR_MEMORY, NULL);
                port->u.file.read_buf_size = size + 1;
            }

            r = read(port->u.file.fd, port->u.file.read_buf, size + 1);
            if (r > 0)
                memcpy(buf, port->u.file.read_buf + 1, (size_t)--r);
        } else {
            r = read(port->u.file.fd, buf, size);
        }
    } else {
        r = read(port->u.file.fd, buf + 1, size - 1);
        if (r > 0) {
            buf[0] = 0;
            r++;
        }
    }
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;

        return hs_error(HS_ERROR_IO, "I/O error while reading from '%s': %s", port->path,
                        strerror(errno));
    }

    return r;
}

ssize_t hs_hid_write(hs_port *port, const uint8_t *buf, size_t size)
{
    assert(port);
    assert(port->type == HS_DEVICE_TYPE_HID);
    assert(port->mode & HS_PORT_MODE_WRITE);
    assert(buf);

    if (size < 2)
        return 0;

    ssize_t r;

restart:
    // On linux, USB requests timeout after 5000ms and O_NONBLOCK isn't honoured for write
    r = write(port->u.file.fd, (const char *)buf, size);
    if (r < 0) {
        if (errno == EINTR)
            goto restart;

        return hs_error(HS_ERROR_IO, "I/O error while writing to '%s': %s", port->path,
                        strerror(errno));
    }

    return r;
}

ssize_t hs_hid_get_feature_report(hs_port *port, uint8_t report_id, uint8_t *buf, size_t size)
{
    assert(port);
    assert(port->type == HS_DEVICE_TYPE_HID);
    assert(port->mode & HS_PORT_MODE_READ);
    assert(buf);
    assert(size);

    ssize_t r;

    if (size >= 2)
        buf[1] = report_id;

restart:
    r = ioctl(port->u.file.fd, HIDIOCGFEATURE(size - 1), (const char *)buf + 1);
    if (r < 0) {
        if (errno == EINTR)
            goto restart;

        return hs_error(HS_ERROR_IO, "I/O error while reading from '%s': %s", port->path,
                        strerror(errno));
    }

    buf[0] = report_id;
    return r + 1;
}

ssize_t hs_hid_send_feature_report(hs_port *port, const uint8_t *buf, size_t size)
{
    assert(port);
    assert(port->type == HS_DEVICE_TYPE_HID);
    assert(port->mode & HS_PORT_MODE_WRITE);
    assert(buf);

    if (size < 2)
        return 0;

    ssize_t r;

restart:
    r = ioctl(port->u.file.fd, HIDIOCSFEATURE(size), (const char *)buf);
    if (r < 0) {
        if (errno == EINTR)
            goto restart;

        return hs_error(HS_ERROR_IO, "I/O error while writing to '%s': %s", port->path,
                        strerror(errno));
    }

    return r;
}

// monitor_linux.c
// ------------------------------------

// #include "common_priv.h"
#include <fcntl.h>
#include <linux/hidraw.h>
#include <libudev.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <unistd.h>
// #include "device_priv.h"
// #include "match_priv.h"
// #include "monitor_priv.h"
// #include "platform.h"

struct hs_monitor {
    _hs_match_helper match_helper;
    _hs_htable devices;

    struct udev_monitor *udev_mon;
    int wait_fd;
};

struct device_subsystem {
    const char *subsystem;
    hs_device_type type;
};

struct udev_aggregate {
    struct udev_device *dev;
    struct udev_device *usb;
    struct udev_device *iface;
};

static struct device_subsystem device_subsystems[] = {
    {"hidraw", HS_DEVICE_TYPE_HID},
    {"tty",    HS_DEVICE_TYPE_SERIAL},
    {NULL}
};

static pthread_mutex_t udev_init_lock = PTHREAD_MUTEX_INITIALIZER;
static struct udev *udev;
static int common_eventfd = -1;

#ifndef _GNU_SOURCE
int dup3(int oldfd, int newfd, int flags);
#endif

static int compute_device_location(struct udev_device *dev, char **rlocation)
{
    const char *busnum, *devpath;
    char *location;
    int r;

    busnum = udev_device_get_sysattr_value(dev, "busnum");
    devpath = udev_device_get_sysattr_value(dev, "devpath");

    if (!busnum || !devpath)
        return 0;

    r = _hs_asprintf(&location, "usb-%s-%s", busnum, devpath);
    if (r < 0)
        return hs_error(HS_ERROR_MEMORY, NULL);

    for (char *ptr = location; *ptr; ptr++) {
        if (*ptr == '.')
            *ptr = '-';
    }

    *rlocation = location;
    return 1;
}

static int fill_device_details(struct udev_aggregate *agg, hs_device *dev)
{
    const char *buf;
    int r;

    buf = udev_device_get_subsystem(agg->dev);
    if (!buf)
        return 0;

    if (strcmp(buf, "hidraw") == 0) {
        dev->type = HS_DEVICE_TYPE_HID;
    } else if (strcmp(buf, "tty") == 0) {
        dev->type = HS_DEVICE_TYPE_SERIAL;
    } else {
        return 0;
    }

    buf = udev_device_get_devnode(agg->dev);
    if (!buf || access(buf, F_OK) != 0)
        return 0;
    dev->path = strdup(buf);
    if (!dev->path)
        return hs_error(HS_ERROR_MEMORY, NULL);

    dev->key = strdup(udev_device_get_devpath(agg->dev));
    if (!dev->key)
        return hs_error(HS_ERROR_MEMORY, NULL);

    r = compute_device_location(agg->usb, &dev->location);
    if (r <= 0)
        return r;

#define READ_UINT16_ATTRIBUTE(name, ptr) \
        do { \
            errno = 0; \
            buf = udev_device_get_sysattr_value(agg->usb, (name)); \
            if (!buf) \
                return 0; \
            *(ptr) = (uint16_t)strtoul(buf, NULL, 16); \
            if (errno) \
                return 0; \
        } while (false)
#define READ_STR_ATTRIBUTE(name, ptr) \
        do { \
            buf = udev_device_get_sysattr_value(agg->usb, (name)); \
            if (buf) { \
                *(ptr) = strdup(buf); \
                if (!*(ptr)) \
                    return hs_error(HS_ERROR_MEMORY, NULL); \
            } \
        } while (false)

    READ_UINT16_ATTRIBUTE("idVendor", &dev->vid);
    READ_UINT16_ATTRIBUTE("idProduct", &dev->pid);
    READ_UINT16_ATTRIBUTE("bcdDevice", &dev->bcd_device);
    READ_STR_ATTRIBUTE("manufacturer", &dev->manufacturer_string);
    READ_STR_ATTRIBUTE("product", &dev->product_string);
    READ_STR_ATTRIBUTE("serial", &dev->serial_number_string);

#undef READ_STR_ATTRIBUTE
#undef READ_UINT16_ATTRIBUTE

    errno = 0;
    buf = udev_device_get_devpath(agg->iface);
    buf += strlen(buf) - 1;
    dev->iface_number = (uint8_t)strtoul(buf, NULL, 10);
    if (errno)
        return 0;

    return 1;
}

static size_t read_hid_descriptor_sysfs(struct udev_aggregate *agg, uint8_t *desc_buf,
                                        size_t desc_buf_size)
{
    struct udev_device *hid_dev;
    char report_path[4096];
    int fd;
    ssize_t r;

    hid_dev = udev_device_get_parent_with_subsystem_devtype(agg->dev, "hid", NULL);
    if (!hid_dev)
        return 0;
    snprintf(report_path, sizeof(report_path), "%s/report_descriptor",
             udev_device_get_syspath(hid_dev));

    fd = open(report_path, O_RDONLY);
    if (fd < 0)
        return 0;
    r = read(fd, desc_buf, desc_buf_size);
    close(fd);
    if (r < 0)
        return 0;

    return (size_t)r;
}

static size_t read_hid_descriptor_hidraw(struct udev_aggregate *agg, uint8_t *desc_buf,
                                         size_t desc_buf_size)
{
    const char *node_path;
    int fd = -1;
    int hidraw_desc_size = 0;
    struct hidraw_report_descriptor hidraw_desc;
    int r;

    node_path = udev_device_get_devnode(agg->dev);
    if (!node_path)
        goto cleanup;
    fd = open(node_path, O_RDONLY);
    if (fd < 0)
        goto cleanup;

    r = ioctl(fd, HIDIOCGRDESCSIZE, &hidraw_desc_size);
    if (r < 0)
        goto cleanup;
    hidraw_desc.size = (uint32_t)hidraw_desc_size;
    r = ioctl(fd, HIDIOCGRDESC, &hidraw_desc);
    if (r < 0) {
        hidraw_desc_size = 0;
        goto cleanup;
    }

    if (desc_buf_size > hidraw_desc.size)
        desc_buf_size = hidraw_desc.size;
    memcpy(desc_buf, hidraw_desc.value, desc_buf_size);

cleanup:
    close(fd);
    return (size_t)hidraw_desc_size;
}

static void parse_hid_descriptor(hs_device *dev, uint8_t *desc, size_t desc_size)
{
    unsigned int collection_depth = 0;

    unsigned int item_size = 0;
    for (size_t i = 0; i < desc_size; i += item_size + 1) {
        unsigned int item_type;
        uint32_t item_data;

        item_type = desc[i];

        if (item_type == 0xFE) {
            // not interested in long items
            if (i + 1 < desc_size)
                item_size = (unsigned int)desc[i + 1] + 2;
            continue;
        }

        item_size = item_type & 3;
        if (item_size == 3)
            item_size = 4;
        item_type &= 0xFC;

        if (i + item_size >= desc_size) {
            hs_log(HS_LOG_WARNING, "Invalid HID descriptor for device '%s'", dev->path);
            return;
        }

        // little endian
        switch (item_size) {
            case 0: {
                item_data = 0;
            } break;
            case 1: {
                item_data = desc[i + 1];
            } break;
            case 2: {
                item_data = (uint32_t)(desc[i + 2] << 8) | desc[i + 1];
            } break;
            case 4: {
                item_data = (uint32_t)((desc[i + 4] << 24) | (desc[i + 3] << 16) |
                                       (desc[i + 2] << 8) | desc[i + 1]);
            } break;

            // silence unitialized warning
            default: {
                item_data = 0;
            } break;
        }

        switch (item_type) {
            // main items
            case 0xA0: {
                collection_depth++;
            } break;
            case 0xC0: {
                collection_depth--;
            } break;

            // global items
            case 0x84: {
                dev->u.hid.numbered_reports = true;
            } break;
            case 0x04: {
                if (!collection_depth)
                    dev->u.hid.usage_page = (uint16_t)item_data;
            } break;

            // local items
            case 0x08: {
                if (!collection_depth)
                    dev->u.hid.usage = (uint16_t)item_data;
            } break;
        }
    }
}

static void fill_hid_properties(struct udev_aggregate *agg, hs_device *dev)
{
    uint8_t desc[HID_MAX_DESCRIPTOR_SIZE];
    size_t desc_size;

    // The sysfs report_descriptor file appeared in 2011, somewhere around Linux 2.6.38
    desc_size = read_hid_descriptor_sysfs(agg, desc, sizeof(desc));
    if (!desc_size) {
        desc_size = read_hid_descriptor_hidraw(agg, desc, sizeof(desc));
        if (!desc_size) {
            // This will happen pretty often on old kernels, most HID nodes are root-only
            hs_log(HS_LOG_DEBUG, "Cannot get HID report descriptor from '%s'", dev->path);
            return;
        }
    }

    parse_hid_descriptor(dev, desc, desc_size);
}

static int read_device_information(struct udev_device *udev_dev, hs_device **rdev)
{
    struct udev_aggregate agg;
    hs_device *dev = NULL;
    int r;

    agg.dev = udev_dev;
    agg.usb = udev_device_get_parent_with_subsystem_devtype(agg.dev, "usb", "usb_device");
    agg.iface = udev_device_get_parent_with_subsystem_devtype(agg.dev, "usb", "usb_interface");
    if (!agg.usb || !agg.iface) {
        r = 0;
        goto cleanup;
    }

    dev = (hs_device *)calloc(1, sizeof(*dev));
    if (!dev) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto cleanup;
    }
    dev->refcount = 1;
    dev->status = HS_DEVICE_STATUS_ONLINE;

    r = fill_device_details(&agg, dev);
    if (r <= 0)
        goto cleanup;

    if (dev->type == HS_DEVICE_TYPE_HID)
        fill_hid_properties(&agg, dev);

    *rdev = dev;
    dev = NULL;

    r = 1;
cleanup:
    hs_device_unref(dev);
    return r;
}

static void release_udev(void)
{
    close(common_eventfd);
    udev_unref(udev);
    pthread_mutex_destroy(&udev_init_lock);
}

static int init_udev(void)
{
    static bool atexit_called;
    int r;

    // fast path
    if (udev && common_eventfd >= 0)
        return 0;

    pthread_mutex_lock(&udev_init_lock);

    if (!atexit_called) {
        atexit(release_udev);
        atexit_called = true;
    }

    if (!udev) {
        udev = udev_new();
        if (!udev) {
            r = hs_error(HS_ERROR_SYSTEM, "udev_new() failed");
            goto cleanup;
        }
    }

    if (common_eventfd < 0) {
        /* We use this as a never-ready placeholder descriptor for all newly created monitors,
           until hs_monitor_start() creates the udev monitor and its socket. */
        common_eventfd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (common_eventfd < 0) {
            r = hs_error(HS_ERROR_SYSTEM, "eventfd() failed: %s", strerror(errno));
            goto cleanup;
        }
    }

    r = 0;
cleanup:
    pthread_mutex_unlock(&udev_init_lock);
    return r;
}

static int enumerate(_hs_match_helper *match_helper, hs_enumerate_func *f, void *udata)
{
    struct udev_enumerate *enumerate;
    int r;

    enumerate = udev_enumerate_new(udev);
    if (!enumerate) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto cleanup;
    }

    udev_enumerate_add_match_is_initialized(enumerate);
    for (unsigned int i = 0; device_subsystems[i].subsystem; i++) {
        if (_hs_match_helper_has_type(match_helper, device_subsystems[i].type)) {
            r = udev_enumerate_add_match_subsystem(enumerate, device_subsystems[i].subsystem);
            if (r < 0) {
                r = hs_error(HS_ERROR_MEMORY, NULL);
                goto cleanup;
            }
        }
    }

    // Current implementation of udev_enumerate_scan_devices() does not fail
    r = udev_enumerate_scan_devices(enumerate);
    if (r < 0) {
        r = hs_error(HS_ERROR_SYSTEM, "udev_enumerate_scan_devices() failed");
        goto cleanup;
    }

    struct udev_list_entry *cur;
    udev_list_entry_foreach(cur, udev_enumerate_get_list_entry(enumerate)) {
        struct udev_device *udev_dev;
        hs_device *dev;

        udev_dev = udev_device_new_from_syspath(udev, udev_list_entry_get_name(cur));
        if (!udev_dev) {
            if (errno == ENOMEM) {
                r = hs_error(HS_ERROR_MEMORY, NULL);
                goto cleanup;
            }
            continue;
        }

        r = read_device_information(udev_dev, &dev);
        udev_device_unref(udev_dev);
        if (r < 0)
            goto cleanup;
        if (!r)
            continue;

        if (_hs_match_helper_match(match_helper, dev, &dev->match_udata)) {
            r = (*f)(dev, udata);
            hs_device_unref(dev);
            if (r)
                goto cleanup;
        } else {
            hs_device_unref(dev);
        }
    }

    r = 0;
cleanup:
    udev_enumerate_unref(enumerate);
    return r;
}

struct enumerate_enumerate_context {
    hs_enumerate_func *f;
    void *udata;
};

static int enumerate_enumerate_callback(hs_device *dev, void *udata)
{
    struct enumerate_enumerate_context *ctx = (struct enumerate_enumerate_context *)udata;

    _hs_device_log(dev, "Enumerate");
    return (*ctx->f)(dev, ctx->udata);
}

int hs_enumerate(const hs_match_spec *matches, unsigned int count, hs_enumerate_func *f,
                 void *udata)
{
    assert(f);

    _hs_match_helper match_helper = {0};
    struct enumerate_enumerate_context ctx;
    int r;

    r = init_udev();
    if (r < 0)
        return r;

    r = _hs_match_helper_init(&match_helper, matches, count);
    if (r < 0)
        return r;

    ctx.f = f;
    ctx.udata = udata;

    r = enumerate(&match_helper, enumerate_enumerate_callback, &ctx);

    _hs_match_helper_release(&match_helper);
    return r;
}

int hs_monitor_new(const hs_match_spec *matches, unsigned int count, hs_monitor **rmonitor)
{
    assert(rmonitor);

    hs_monitor *monitor;
    int r;

    monitor = (hs_monitor *)calloc(1, sizeof(*monitor));
    if (!monitor) {
        r = hs_error(HS_ERROR_MEMORY, NULL);
        goto error;
    }
    monitor->wait_fd = -1;

    r = _hs_match_helper_init(&monitor->match_helper, matches, count);
    if (r < 0)
        goto error;

    r = _hs_htable_init(&monitor->devices, 64);
    if (r < 0)
        goto error;

    r = init_udev();
    if (r < 0)
        goto error;

    monitor->wait_fd = fcntl(common_eventfd, F_DUPFD_CLOEXEC, 0);
    if (monitor->wait_fd < 0) {
        r = hs_error(HS_ERROR_SYSTEM, "fcntl(F_DUPFD_CLOEXEC) failed: %s", strerror(errno));
        goto error;
    }

    *rmonitor = monitor;
    return 0;

error:
    hs_monitor_free(monitor);
    return r;
}

void hs_monitor_free(hs_monitor *monitor)
{
    if (monitor) {
        close(monitor->wait_fd);
        udev_monitor_unref(monitor->udev_mon);

        _hs_monitor_clear_devices(&monitor->devices);
        _hs_htable_release(&monitor->devices);
        _hs_match_helper_release(&monitor->match_helper);
    }

    free(monitor);
}

static int monitor_enumerate_callback(hs_device *dev, void *udata)
{
    hs_monitor *monitor = (hs_monitor *)udata;
    return _hs_monitor_add(&monitor->devices, dev, NULL, NULL);
}

int hs_monitor_start(hs_monitor *monitor)
{
    assert(monitor);

    int r;

    if (monitor->udev_mon)
        return 0;

    monitor->udev_mon = udev_monitor_new_from_netlink(udev, "udev");
    if (!monitor->udev_mon) {
        r = hs_error(HS_ERROR_SYSTEM, "udev_monitor_new_from_netlink() failed");
        goto error;
    }

    for (unsigned int i = 0; device_subsystems[i].subsystem; i++) {
        if (_hs_match_helper_has_type(&monitor->match_helper, device_subsystems[i].type)) {
            r = udev_monitor_filter_add_match_subsystem_devtype(monitor->udev_mon, device_subsystems[i].subsystem, NULL);
            if (r < 0) {
                r = hs_error(HS_ERROR_SYSTEM, "udev_monitor_filter_add_match_subsystem_devtype() failed");
                goto error;
            }
        }
    }

    r = udev_monitor_enable_receiving(monitor->udev_mon);
    if (r < 0) {
        r = hs_error(HS_ERROR_SYSTEM, "udev_monitor_enable_receiving() failed");
        goto error;
    }

    r = enumerate(&monitor->match_helper, monitor_enumerate_callback, monitor);
    if (r < 0)
        goto error;

    /* Given the documentation of dup3() and the kernel code handling it, I'm reasonably sure
       nothing can make this call fail. */
    dup3(udev_monitor_get_fd(monitor->udev_mon), monitor->wait_fd, O_CLOEXEC);

    return 0;

error:
    hs_monitor_stop(monitor);
    return r;
}

void hs_monitor_stop(hs_monitor *monitor)
{
    assert(monitor);

    if (!monitor->udev_mon)
        return;

    _hs_monitor_clear_devices(&monitor->devices);

    dup3(common_eventfd, monitor->wait_fd, O_CLOEXEC);
    udev_monitor_unref(monitor->udev_mon);
    monitor->udev_mon = NULL;
}

hs_handle hs_monitor_get_poll_handle(const hs_monitor *monitor)
{
    assert(monitor);
    return monitor->wait_fd;
}

int hs_monitor_refresh(hs_monitor *monitor, hs_enumerate_func *f, void *udata)
{
    assert(monitor);

    struct udev_device *udev_dev;
    int r;

    if (!monitor->udev_mon)
        return 0;

    errno = 0;
    while ((udev_dev = udev_monitor_receive_device(monitor->udev_mon))) {
        const char *action = udev_device_get_action(udev_dev);

        r = 0;
        if (strcmp(action, "add") == 0) {
            hs_device *dev = NULL;

            r = read_device_information(udev_dev, &dev);
            if (r > 0) {
                r = _hs_match_helper_match(&monitor->match_helper, dev, &dev->match_udata);
                if (r)
                    r = _hs_monitor_add(&monitor->devices, dev, f, udata);
            }

            hs_device_unref(dev);
        } else if (strcmp(action, "remove") == 0) {
            _hs_monitor_remove(&monitor->devices, udev_device_get_devpath(udev_dev), f, udata);
        }
        udev_device_unref(udev_dev);
        if (r)
            return r;

        errno = 0;
    }
    if (errno == ENOMEM)
        return hs_error(HS_ERROR_MEMORY, NULL);

    return 0;
}

int hs_monitor_list(hs_monitor *monitor, hs_enumerate_func *f, void *udata)
{
    return _hs_monitor_list(&monitor->devices, f, udata);
}

// platform_posix.c
// ------------------------------------

// #include "common_priv.h"
#include <poll.h>
#include <sys/utsname.h>
#include <time.h>
// #include "platform.h"

uint64_t hs_millis(void)
{
    struct timespec ts;
    int r _HS_POSSIBLY_UNUSED;

#ifdef CLOCK_MONOTONIC_RAW
    r = clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#else
    r = clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
    assert(!r);

    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 10000000;
}

void hs_delay(unsigned int ms)
{
    struct timespec t, rem;
    int r;

    t.tv_sec = (int)(ms / 1000);
    t.tv_nsec = (int)((ms % 1000) * 1000000);

    do {
        r = nanosleep(&t, &rem);
        if (r < 0) {
            if (errno != EINTR) {
                hs_error(HS_ERROR_SYSTEM, "nanosleep() failed: %s", strerror(errno));
                return;
            }

            t = rem;
        }
    } while (r);
}

int hs_poll(hs_poll_source *sources, unsigned int count, int timeout)
{
    assert(sources);
    assert(count);
    assert(count <= HS_POLL_MAX_SOURCES);

    struct pollfd pfd[HS_POLL_MAX_SOURCES];
    uint64_t start;
    int r;

    for (unsigned int i = 0; i < count; i++) {
        pfd[i].fd = sources[i].desc;
        pfd[i].events = POLLIN;
        sources[i].ready = 0;
    }

    start = hs_millis();
restart:
    r = poll(pfd, (nfds_t)count, hs_adjust_timeout(timeout, start));
    if (r < 0) {
        if (errno == EINTR)
            goto restart;
        return hs_error(HS_ERROR_SYSTEM, "poll() failed: %s", strerror(errno));
    }
    if (!r)
        return 0;

    for (unsigned int i = 0; i < count; i++)
        sources[i].ready = !!pfd[i].revents;

    return r;
}

#ifdef __linux__
uint32_t hs_linux_version(void)
{
    static uint32_t version;

    if (!version) {
        struct utsname name;
        uint32_t major = 0, minor = 0, release = 0, patch = 0;

        uname(&name);
        sscanf(name.release, "%u.%u.%u.%u", &major, &minor, &release, &patch);
        if (major >= 3) {
            patch = release;
            release = 0;
        }

        version = major * 10000000 + minor * 100000 + release * 1000 + patch;
    }

    return version;
}
#endif

// serial_posix.c
// ------------------------------------

// #include "common_priv.h"
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
// #include "device_priv.h"
// #include "platform.h"
// #include "serial.h"

int hs_serial_set_config(hs_port *port, const hs_serial_config *config)
{
    assert(port);
    assert(config);

    struct termios tio;
    int modem_bits;
    int r;

    r = tcgetattr(port->u.file.fd, &tio);
    if (r < 0)
        return hs_error(HS_ERROR_SYSTEM, "Unable to get serial port settings from '%s': %s",
                        port->path, strerror(errno));
    r = ioctl(port->u.file.fd, TIOCMGET, &modem_bits);
    if (r < 0)
        return hs_error(HS_ERROR_SYSTEM, "Unable to get modem bits from '%s': %s",
                        port->path, strerror(errno));

    if (config->baudrate) {
        speed_t std_baudrate;

        switch (config->baudrate) {
            case 110: { std_baudrate = B110; } break;
            case 134: { std_baudrate = B134; } break;
            case 150: { std_baudrate = B150; } break;
            case 200: { std_baudrate = B200; } break;
            case 300: { std_baudrate = B300; } break;
            case 600: { std_baudrate = B600; } break;
            case 1200: { std_baudrate = B1200; } break;
            case 1800: { std_baudrate = B1800; } break;
            case 2400: { std_baudrate = B2400; } break;
            case 4800: { std_baudrate = B4800; } break;
            case 9600: { std_baudrate = B9600; } break;
            case 19200: { std_baudrate = B19200; } break;
            case 38400: { std_baudrate = B38400; } break;
            case 57600: { std_baudrate = B57600; } break;
            case 115200: { std_baudrate = B115200; } break;
            case 230400: { std_baudrate = B230400; } break;

            default: {
                return hs_error(HS_ERROR_SYSTEM, "Unsupported baud rate value: %u",
                                config->baudrate);
            } break;
        }

        cfsetispeed(&tio, std_baudrate);
        cfsetospeed(&tio, std_baudrate);
    }

    if (config->databits) {
        tio.c_cflag &= (unsigned int)~CSIZE;

        switch (config->databits) {
            case 5: { tio.c_cflag |= CS5; } break;
            case 6: { tio.c_cflag |= CS6; } break;
            case 7: { tio.c_cflag |= CS7; } break;
            case 8: { tio.c_cflag |= CS8; } break;

            default: {
                return hs_error(HS_ERROR_SYSTEM, "Invalid data bits setting: %u",
                                config->databits);
            } break;
        }
    }

    if (config->stopbits) {
        tio.c_cflag &= (unsigned int)~CSTOPB;

        switch (config->stopbits) {
            case 1: {} break;
            case 2: { tio.c_cflag |= CSTOPB; } break;

            default: {
                return hs_error(HS_ERROR_SYSTEM, "Invalid stop bits setting: %u",
                                config->stopbits);
            } break;
        }
    }

    if (config->parity) {
        tio.c_cflag &= (unsigned int)~(PARENB | PARODD);
#ifdef CMSPAR
        tio.c_cflag &= (unsigned int)~CMSPAR;
#endif

        switch (config->parity) {
            case HS_SERIAL_CONFIG_PARITY_OFF: {} break;
            case HS_SERIAL_CONFIG_PARITY_EVEN: { tio.c_cflag |= PARENB; } break;
            case HS_SERIAL_CONFIG_PARITY_ODD: { tio.c_cflag |= PARENB | PARODD; } break;
#ifdef CMSPAR
            case HS_SERIAL_CONFIG_PARITY_SPACE: { tio.c_cflag |= PARENB | CMSPAR; } break;
            case HS_SERIAL_CONFIG_PARITY_MARK: { tio.c_cflag |= PARENB | PARODD | CMSPAR; } break;
#else

            case HS_SERIAL_CONFIG_PARITY_MARK:
            case HS_SERIAL_CONFIG_PARITY_SPACE: {
                return hs_error(HS_ERROR_SYSTEM, "Mark/space parity is not supported");
            } break;
#endif
            default: {
                return hs_error(HS_ERROR_SYSTEM, "Invalid parity setting: %d", config->parity);
            } break;
        }
    }

    if (config->rts) {
        tio.c_cflag &= (unsigned int)~CRTSCTS;
        modem_bits &= ~TIOCM_RTS;

        switch (config->rts) {
            case HS_SERIAL_CONFIG_RTS_OFF: {} break;
            case HS_SERIAL_CONFIG_RTS_ON: { modem_bits |= TIOCM_RTS; } break;
            case HS_SERIAL_CONFIG_RTS_FLOW: { tio.c_cflag |= CRTSCTS; } break;

            default: {
                return hs_error(HS_ERROR_SYSTEM, "Invalid RTS setting: %d", config->rts);
            } break;
        }
    }

    switch (config->dtr) {
        case 0: {} break;
        case HS_SERIAL_CONFIG_DTR_OFF: { modem_bits &= ~TIOCM_DTR; } break;
        case HS_SERIAL_CONFIG_DTR_ON: { modem_bits |= TIOCM_DTR; } break;

        default: {
            return hs_error(HS_ERROR_SYSTEM, "Invalid DTR setting: %d", config->dtr);
        } break;
    }

    if (config->xonxoff) {
        tio.c_iflag &= (unsigned int)~(IXON | IXOFF | IXANY);

        switch (config->xonxoff) {
            case HS_SERIAL_CONFIG_XONXOFF_OFF: {} break;
            case HS_SERIAL_CONFIG_XONXOFF_IN: { tio.c_iflag |= IXOFF; } break;
            case HS_SERIAL_CONFIG_XONXOFF_OUT: { tio.c_iflag |= IXON | IXANY; } break;
            case HS_SERIAL_CONFIG_XONXOFF_INOUT: { tio.c_iflag |= IXOFF | IXON | IXANY; } break;

            default: {
                return hs_error(HS_ERROR_SYSTEM, "Invalid XON/XOFF setting: %d", config->xonxoff);
            } break;
        }
    }

    r = ioctl(port->u.file.fd, TIOCMSET, &modem_bits);
    if (r < 0)
        return hs_error(HS_ERROR_SYSTEM, "Unable to set modem bits of '%s': %s",
                        port->path, strerror(errno));
    r = tcsetattr(port->u.file.fd, TCSANOW, &tio);
    if (r < 0)
        return hs_error(HS_ERROR_SYSTEM, "Unable to change serial port settings of '%s': %s",
                        port->path, strerror(errno));

    return 0;
}

int hs_serial_get_config(hs_port *port, hs_serial_config *config)
{
    assert(port);

    struct termios tio;
    int modem_bits;
    int r;

    r = tcgetattr(port->u.file.fd, &tio);
    if (r < 0)
        return hs_error(HS_ERROR_SYSTEM, "Unable to read port settings from '%s': %s",
                        port->path, strerror(errno));
    r = ioctl(port->u.file.fd, TIOCMGET, &modem_bits);
    if (r < 0)
        return hs_error(HS_ERROR_SYSTEM, "Unable to get modem bits from '%s': %s",
                        port->path, strerror(errno));

    /* 0 is the INVALID value for all parameters, we keep that value if we can't interpret
       a termios value (only a cross-platform subset of it is exposed in hs_serial_config). */
    memset(config, 0, sizeof(*config));

    switch (cfgetispeed(&tio)) {
        case B110: { config->baudrate = 110; } break;
        case B134: { config->baudrate = 134; } break;
        case B150: { config->baudrate = 150; } break;
        case B200: { config->baudrate = 200; } break;
        case B300: { config->baudrate = 300; } break;
        case B600: { config->baudrate = 600; } break;
        case B1200: { config->baudrate = 1200; } break;
        case B1800: { config->baudrate = 1800; } break;
        case B2400: { config->baudrate = 2400; } break;
        case B4800: { config->baudrate = 4800; } break;
        case B9600: { config->baudrate = 9600; } break;
        case B19200: { config->baudrate = 19200; } break;
        case B38400: { config->baudrate = 38400; } break;
        case B57600: { config->baudrate = 57600; } break;
        case B115200: { config->baudrate = 115200; } break;
        case B230400: { config->baudrate = 230400; } break;
    }

    switch (tio.c_cflag & CSIZE) {
        case CS5: { config->databits = 5; } break;
        case CS6: { config->databits = 6; } break;
        case CS7: { config->databits = 7; } break;
        case CS8: { config->databits = 8; } break;
    }

    if (tio.c_cflag & CSTOPB) {
        config->stopbits = 2;
    } else {
        config->stopbits = 1;
    }

    // FIXME: should we detect IGNPAR here?
    if (tio.c_cflag & PARENB) {
#ifdef CMSPAR
        switch (tio.c_cflag & (PARODD | CMSPAR)) {
#else
        switch (tio.c_cflag & PARODD) {
#endif
            case 0: { config->parity = HS_SERIAL_CONFIG_PARITY_EVEN; } break;
            case PARODD: { config->parity = HS_SERIAL_CONFIG_PARITY_ODD; } break;
#ifdef CMSPAR
            case CMSPAR: { config->parity = HS_SERIAL_CONFIG_PARITY_SPACE; } break;
            case CMSPAR | PARODD: { config->parity = HS_SERIAL_CONFIG_PARITY_MARK; } break;
#endif
        }
    } else {
        config->parity = HS_SERIAL_CONFIG_PARITY_OFF;
    }

    if (tio.c_cflag & CRTSCTS) {
        config->rts = HS_SERIAL_CONFIG_RTS_FLOW;
    } else if (modem_bits & TIOCM_RTS) {
        config->rts = HS_SERIAL_CONFIG_RTS_ON;
    } else {
        config->rts = HS_SERIAL_CONFIG_RTS_OFF;
    }

    if (modem_bits & TIOCM_DTR) {
        config->dtr = HS_SERIAL_CONFIG_DTR_ON;
    } else {
        config->dtr = HS_SERIAL_CONFIG_DTR_OFF;
    }

    switch (tio.c_iflag & (IXON | IXOFF)) {
        case 0: { config->xonxoff = HS_SERIAL_CONFIG_XONXOFF_OFF; } break;
        case IXOFF: { config->xonxoff = HS_SERIAL_CONFIG_XONXOFF_IN; } break;
        case IXON: { config->xonxoff = HS_SERIAL_CONFIG_XONXOFF_OUT; } break;
        case IXOFF | IXON: { config->xonxoff = HS_SERIAL_CONFIG_XONXOFF_INOUT; } break;
    }

    return 0;
}

ssize_t hs_serial_read(hs_port *port, uint8_t *buf, size_t size, int timeout)
{
    assert(port);
    assert(port->type == HS_DEVICE_TYPE_SERIAL);
    assert(port->mode & HS_PORT_MODE_READ);
    assert(buf);
    assert(size);

    ssize_t r;

    if (timeout) {
        struct pollfd pfd;
        uint64_t start;

        pfd.events = POLLIN;
        pfd.fd = port->u.file.fd;

        start = hs_millis();
restart:
        r = poll(&pfd, 1, hs_adjust_timeout(timeout, start));
        if (r < 0) {
            if (errno == EINTR)
                goto restart;

            return hs_error(HS_ERROR_IO, "I/O error while reading from '%s': %s", port->path,
                            strerror(errno));
        }
        if (!r)
            return 0;
    }

    r = read(port->u.file.fd, buf, size);
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;

        return hs_error(HS_ERROR_IO, "I/O error while reading from '%s': %s", port->path,
                        strerror(errno));
    }

    return r;
}

ssize_t hs_serial_write(hs_port *port, const uint8_t *buf, size_t size, int timeout)
{
    assert(port);
    assert(port->type == HS_DEVICE_TYPE_SERIAL);
    assert(port->mode & HS_PORT_MODE_WRITE);
    assert(buf);

    struct pollfd pfd;
    uint64_t start;
    int adjusted_timeout;
    size_t written;

    pfd.events = POLLOUT;
    pfd.fd = port->u.file.fd;

    start = hs_millis();
    adjusted_timeout = timeout;

    written = 0;
    do {
        ssize_t r;

        r = poll(&pfd, 1, adjusted_timeout);
        if (r < 0) {
            if (errno == EINTR)
                continue;

            return hs_error(HS_ERROR_IO, "I/O error while writing to '%s': %s", port->path,
                            strerror(errno));
        }
        if (!r)
            break;

        r = write(port->u.file.fd, buf + written, size - written);
        if (r < 0) {
            if (errno == EINTR)
                continue;

            return hs_error(HS_ERROR_IO, "I/O error while writing to '%s': %s", port->path,
                            strerror(errno));
        }
        written += (size_t)r;

        adjusted_timeout = hs_adjust_timeout(timeout, start);
    } while (written < size && adjusted_timeout);

    return (ssize_t)written;
}

    #else
        #error "Platform not supported"
    #endif
#endif
