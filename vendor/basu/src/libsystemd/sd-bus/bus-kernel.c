/* SPDX-License-Identifier: LGPL-2.1+ */

#include <fcntl.h>
#include <sys/mman.h>

/* When we include libgen.h because we need dirname() we immediately
 * undefine basename() since libgen.h defines it as a macro to the POSIX
 * version which is really broken. We prefer GNU basename(). */
#include <libgen.h>
#undef basename

#include "alloc-util.h"
#include "bus-internal.h"
#include "fd-util.h"

void close_and_munmap(int fd, void *address, size_t size) {
        if (size > 0)
                assert_se(munmap(address, PAGE_ALIGN(size)) >= 0);

        safe_close(fd);
}

void bus_flush_memfd(sd_bus *b) {
        unsigned i;

        assert(b);

        for (i = 0; i < b->n_memfd_cache; i++)
                close_and_munmap(b->memfd_cache[i].fd, b->memfd_cache[i].address, b->memfd_cache[i].mapped);
}
