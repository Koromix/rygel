/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/uio.h>

#include "time-util.h"

ssize_t loop_read(int fd, void *buf, size_t nbytes, bool do_poll);
int loop_read_exact(int fd, void *buf, size_t nbytes, bool do_poll);

int fd_wait_for_event(int fd, int event, usec_t timeout);

static inline size_t IOVEC_TOTAL_SIZE(const struct iovec *i, size_t n) {
        size_t j, r = 0;

        for (j = 0; j < n; j++)
                r += i[j].iov_len;

        return r;
}

#define IOVEC_INIT(base, len) { .iov_base = (base), .iov_len = (len) }
#define IOVEC_INIT_STRING(string) IOVEC_INIT((char*) string, strlen(string))
#define IOVEC_MAKE_STRING(string) (struct iovec) IOVEC_INIT_STRING(string)
