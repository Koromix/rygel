/* SPDX-License-Identifier: LGPL-2.1+ */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "alloc-util.h"
#include "def.h"
#include "escape.h"
#include "fd-util.h"
#include "fileio.h"
#include "string-util.h"
#include "utf8.h"

#define READ_FULL_BYTES_MAX (4U*1024U*1024U)

int read_one_line_file(const char *fn, char **line) {
        _cleanup_fclose_ FILE *f = NULL;
        int r;

        assert(fn);
        assert(line);

        f = fopen(fn, "re");
        if (!f)
                return -errno;

        r = read_line(f, LONG_LINE_MAX, line);
        return r < 0 ? r : 0;
}

int read_full_stream(FILE *f, char **contents, size_t *size) {
        _cleanup_free_ char *buf = NULL;
        struct stat st;
        size_t n, l;
        int fd;

        assert(f);
        assert(contents);

        n = LINE_MAX;

        fd = fileno(f);
        if (fd >= 0) { /* If the FILE* object is backed by an fd (as opposed to memory or such, see fmemopen(), let's
                        * optimize our buffering) */

                if (fstat(fileno(f), &st) < 0)
                        return -errno;

                if (S_ISREG(st.st_mode)) {

                        /* Safety check */
                        if (st.st_size > READ_FULL_BYTES_MAX)
                                return -E2BIG;

                        /* Start with the right file size, but be prepared for files from /proc which generally report a file
                         * size of 0. Note that we increase the size to read here by one, so that the first read attempt
                         * already makes us notice the EOF. */
                        if (st.st_size > 0)
                                n = st.st_size + 1;
                }
        }

        l = 0;
        for (;;) {
                char *t;
                size_t k;

                t = realloc(buf, n + 1);
                if (!t)
                        return -ENOMEM;

                buf = t;
                errno = 0;
                k = fread(buf + l, 1, n - l, f);
                if (k > 0)
                        l += k;

                if (ferror(f))
                        return errno > 0 ? -errno : -EIO;

                if (feof(f))
                        break;

                /* We aren't expecting fread() to return a short read outside
                 * of (error && eof), assert buffer is full and enlarge buffer.
                 */
                assert(l == n);

                /* Safety check */
                if (n >= READ_FULL_BYTES_MAX)
                        return -E2BIG;

                n = MIN(n * 2, READ_FULL_BYTES_MAX);
        }

        buf[l] = 0;
        *contents = TAKE_PTR(buf);

        if (size)
                *size = l;

        return 0;
}

int read_full_file(const char *fn, char **contents, size_t *size) {
        _cleanup_fclose_ FILE *f = NULL;

        assert(fn);
        assert(contents);

        f = fopen(fn, "re");
        if (!f)
                return -errno;

        return read_full_stream(f, contents, size);
}

int fflush_and_check(FILE *f) {
        assert(f);

        errno = 0;
        fflush(f);

        if (ferror(f))
                return errno > 0 ? -errno : -EIO;

        return 0;
}

DEFINE_TRIVIAL_CLEANUP_FUNC(FILE*, funlockfile);

int read_line(FILE *f, size_t limit, char **ret) {
        _cleanup_free_ char *buffer = NULL;
        size_t n = 0, allocated = 0, count = 0;

        assert(f);

        /* Something like a bounded version of getline().
         *
         * Considers EOF, \n and \0 end of line delimiters, and does not include these delimiters in the string
         * returned.
         *
         * Returns the number of bytes read from the files (i.e. including delimiters — this hence usually differs from
         * the number of characters in the returned string). When EOF is hit, 0 is returned.
         *
         * The input parameter limit is the maximum numbers of characters in the returned string, i.e. excluding
         * delimiters. If the limit is hit we fail and return -ENOBUFS.
         *
         * If a line shall be skipped ret may be initialized as NULL. */

        if (ret) {
                if (!GREEDY_REALLOC(buffer, allocated, 1))
                        return -ENOMEM;
        }

        {
                _unused_ _cleanup_(funlockfilep) FILE *flocked = f;
                flockfile(f);

                for (;;) {
                        int c;

                        if (n >= limit)
                                return -ENOBUFS;

                        errno = 0;
                        c = fgetc(f);
                        if (c == EOF) {
                                /* if we read an error, and have no data to return, then propagate the error */
                                if (ferror_unlocked(f) && n == 0)
                                        return errno > 0 ? -errno : -EIO;

                                break;
                        }

                        count++;

                        if (IN_SET(c, '\n', 0)) /* Reached a delimiter */
                                break;

                        if (ret) {
                                if (!GREEDY_REALLOC(buffer, allocated, n + 2))
                                        return -ENOMEM;

                                buffer[n] = (char) c;
                        }

                        n++;
                }
        }

        if (ret) {
                buffer[n] = 0;

                *ret = TAKE_PTR(buffer);
        }

        return (int) count;
}
