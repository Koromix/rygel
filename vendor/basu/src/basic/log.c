/* SPDX-License-Identifier: LGPL-2.1+ */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>

#include "alloc-util.h"
#include "io-util.h"
#include "process-util.h"
#include "socket-util.h"
#include "stdio-util.h"
#include "string-table.h"
#include "syslog-util.h"
#include "util.h"

static int log_max_level[] = {LOG_INFO, LOG_INFO};
assert_cc(ELEMENTSOF(log_max_level) == _LOG_REALM_MAX);


/* An assert to use in logging functions that does not call recursively
 * into our logging functions (since that might lead to a loop). */
#define assert_raw(expr)                                                \
        do {                                                            \
                if (_unlikely_(!(expr))) {                              \
                        fputs(#expr "\n", stderr);                      \
                        abort();                                        \
                }                                                       \
        } while (false)

void log_set_max_level_realm(LogRealm realm, int level) {
        assert((level & LOG_PRIMASK) == level);
        assert(realm < ELEMENTSOF(log_max_level));

        log_max_level[realm] = level;
}

static int write_to_console(
                int level,
                int error,
                const char *file,
                int line,
                const char *func,
                const char *buffer) {

        char location[256], prefix[1 + DECIMAL_STR_MAX(int) + 2];
        struct iovec iovec[6] = {};
        size_t n = 0;

        xsprintf(prefix, "<%i>", level);
        iovec[n++] = IOVEC_MAKE_STRING(prefix);

        (void) snprintf(location, sizeof location, "(%s:%i) ", file, line);
        iovec[n++] = IOVEC_MAKE_STRING(location);

        iovec[n++] = IOVEC_MAKE_STRING(buffer);
        iovec[n++] = IOVEC_MAKE_STRING("\n");

        if (writev(STDERR_FILENO, iovec, n) < 0) {
                return -errno;
        }

        return 1;
}

static int log_dispatch_internal(
                int level,
                int error,
                const char *file,
                int line,
                const char *func,
                const char *object_field,
                const char *object,
                const char *extra_field,
                const char *extra,
                char *buffer) {

        assert_raw(buffer);

        if (error < 0)
                error = -error;

        /* Patch in LOG_DAEMON facility if necessary */
        if ((level & LOG_FACMASK) == 0)
                level = LOG_DAEMON | LOG_PRI(level);
        do {
                char *e;

                buffer += strspn(buffer, NEWLINE);

                if (buffer[0] == 0)
                        break;

                if ((e = strpbrk(buffer, NEWLINE)))
                        *(e++) = 0;

                (void) write_to_console(level, error, file, line, func, buffer);

                buffer = e;
        } while (buffer);

        return -error;
}

static int log_internalv_realm(
                int level,
                int error,
                const char *file,
                int line,
                const char *func,
                const char *format,
                va_list ap) {

        LogRealm realm = LOG_REALM_REMOVE_LEVEL(level);
        char buffer[LINE_MAX];
        PROTECT_ERRNO;

        if (error < 0)
                error = -error;

        if (_likely_(LOG_PRI(level) > log_max_level[realm]))
                return -error;

        /* Make sure that %m maps to the specified error (or "Success"). */
        errno = error;

        (void) vsnprintf(buffer, sizeof buffer, format, ap);

        return log_dispatch_internal(level, error, file, line, func, NULL, NULL, NULL, NULL, buffer);
}

int log_internal_realm(
                int level,
                int error,
                const char *file,
                int line,
                const char *func,
                const char *format, ...) {

        va_list ap;
        int r;

        va_start(ap, format);
        r = log_internalv_realm(level, error, file, line, func, format, ap);
        va_end(ap);

        return r;
}

static void log_assert(
                int level,
                const char *text,
                const char *file,
                int line,
                const char *func,
                const char *format) {

        static char buffer[LINE_MAX];
        LogRealm realm = LOG_REALM_REMOVE_LEVEL(level);

        if (_likely_(LOG_PRI(level) > log_max_level[realm]))
                return;

        DISABLE_WARNING_FORMAT_NONLITERAL;
        (void) snprintf(buffer, sizeof buffer, format, text, file, line, func);
        REENABLE_WARNING;

        log_dispatch_internal(level, 0, file, line, func, NULL, NULL, NULL, NULL, buffer);
}

_noreturn_ void log_assert_failed_realm(
                LogRealm realm,
                const char *text,
                const char *file,
                int line,
                const char *func) {
        log_assert(LOG_REALM_PLUS_LEVEL(realm, LOG_CRIT), text, file, line, func,
                   "Assertion '%s' failed at %s:%u, function %s(). Aborting.");
        abort();
}

_noreturn_ void log_assert_failed_unreachable_realm(
                LogRealm realm,
                const char *text,
                const char *file,
                int line,
                const char *func) {
        log_assert(LOG_REALM_PLUS_LEVEL(realm, LOG_CRIT), text, file, line, func,
                   "Code should not be reached '%s' at %s:%u, function %s(). Aborting.");
        abort();
}

void log_assert_failed_return_realm(
                LogRealm realm,
                const char *text,
                const char *file,
                int line,
                const char *func) {
        PROTECT_ERRNO;
        log_assert(LOG_REALM_PLUS_LEVEL(realm, LOG_DEBUG), text, file, line, func,
                   "Assertion '%s' failed at %s:%u, function %s(). Ignoring.");
}

int log_oom_internal(LogRealm realm, const char *file, int line, const char *func) {
        return log_internal_realm(LOG_REALM_PLUS_LEVEL(realm, LOG_ERR),
                                  ENOMEM, file, line, func, "Out of memory.");
}

static int log_set_max_level_from_string_realm(LogRealm realm, const char *e) {
        int t;

        t = log_level_from_string(e);
        if (t < 0)
                return -EINVAL;

        log_set_max_level_realm(realm, t);
        return 0;
}

void log_parse_environment_realm(LogRealm realm) {
        /* Do not call from library code. */

        const char *e;

        e = getenv("SYSTEMD_LOG_LEVEL");
        if (e && log_set_max_level_from_string_realm(realm, e) < 0)
                log_warning("Failed to parse log level '%s'. Ignoring.", e);
}

int log_get_max_level_realm(LogRealm realm) {
        return log_max_level[realm];
}

