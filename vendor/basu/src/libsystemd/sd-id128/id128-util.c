/* SPDX-License-Identifier: LGPL-2.1+ */

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "fd-util.h"
#include "fs-util.h"
#include "id128-util.h"
#include "io-util.h"
#include "missing.h"

int id128_read_fd(int fd, Id128Format f, sd_id128_t *ret) {
        char buffer[36 + 2];
        ssize_t l;

        assert(fd >= 0);
        assert(f < _ID128_FORMAT_MAX);

        /* Reads an 128bit ID from a file, which may either be in plain format (32 hex digits), or in UUID format, both
         * optionally followed by a newline and nothing else. ID files should really be newline terminated, but if they
         * aren't that's OK too, following the rule of "Be conservative in what you send, be liberal in what you
         * accept". */

        l = loop_read(fd, buffer, sizeof(buffer), false); /* we expect a short read of either 32/33 or 36/37 chars */
        if (l < 0)
                return (int) l;
        if (l == 0) /* empty? */
                return -ENOMEDIUM;

        switch (l) {

        case 33: /* plain UUID with trailing newline */
                if (buffer[32] != '\n')
                        return -EINVAL;

                _fallthrough_;
        case 32: /* plain UUID without trailing newline */
                if (f == ID128_UUID)
                        return -EINVAL;

                buffer[32] = 0;
                break;

        case 37: /* RFC UUID with trailing newline */
                if (buffer[36] != '\n')
                        return -EINVAL;

                _fallthrough_;
        case 36: /* RFC UUID without trailing newline */
                if (f == ID128_PLAIN)
                        return -EINVAL;

                buffer[36] = 0;
                break;

        default:
                return -EINVAL;
        }

        return sd_id128_from_string(buffer, ret);
}

int id128_read(const char *p, Id128Format f, sd_id128_t *ret) {
        _cleanup_close_ int fd = -1;

        fd = open(p, O_RDONLY|O_CLOEXEC|O_NOCTTY);
        if (fd < 0)
                return -errno;

        return id128_read_fd(fd, f, ret);
}

void id128_hash_func(const void *p, struct siphash *state) {
        siphash24_compress(p, 16, state);
}

int id128_compare_func(const void *a, const void *b) {
        return memcmp(a, b, 16);
}

const struct hash_ops id128_hash_ops = {
        .hash = id128_hash_func,
        .compare = id128_compare_func,
};
