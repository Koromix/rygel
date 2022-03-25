/* SPDX-License-Identifier: LGPL-2.1+ */

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>

#include "io-util.h"
#include "random-util.h"

int random_bytes(void *p, size_t n) {
        int fd = open("/dev/urandom", O_RDONLY|O_CLOEXEC|O_NOCTTY);
        if (fd < 0)
                return errno == ENOENT ? -ENOSYS : -errno;
        return loop_read_exact(fd, p, n, true);
}

